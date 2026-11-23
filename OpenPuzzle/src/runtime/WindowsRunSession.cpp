#include "openpuzzle/runtime/WindowsRunSession.hpp"
#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/runtime/ExecutionSlot.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/core/WindowsVersion.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/WindowsVanityRun.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
namespace openpuzzle {
namespace fs=std::filesystem;using Store=client::ClientStateStore;using Control=ClientRuntimeControl;
namespace {
bool present(const fs::path& p){std::error_code e;auto s=fs::symlink_status(p,e);return (e&&e!=std::errc::no_such_file_or_directory)||fs::exists(s);}
std::string canonical(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
std::string receipt(const client::RangeAssignment& a,const std::string& id,const std::string& url,const RunCommandOptions& o) {
 auto quote=[](const std::string& s){return "\""+escapeJsonString(s)+"\"";};
 std::ostringstream value;value<<"{\"version\":1,\"client_id\":"<<quote(id)<<",\"server\":"<<quote(url)
   <<",\"assignment_id\":"<<quote(a.assignmentId)<<",\"puzzle\":"<<a.puzzle<<",\"range_id\":"<<a.rangeId
   <<",\"start\":"<<quote(a.start)<<",\"end\":"<<quote(a.end)<<",\"target\":"<<quote(a.target)
   <<",\"search_mode\":"<<quote(a.searchMode)<<",\"public_key\":"<<quote(a.publicKey)
   <<",\"required_backend\":"<<quote(a.requiredBackend)<<",\"backend\":"<<quote(o.backend)<<",\"engine\":"<<quote(o.engine)<<"}\n";return value.str();
}

void verifyVanityReceipt(const std::string& text, const client::ClientExecutionState& state,
                        const std::string& server) {
 boost::property_tree::ptree fields;
 std::istringstream input(text);
 boost::property_tree::read_json(input, fields);
 std::set<std::string> seen;
 for (const auto& item : fields) {
   if (!item.second.empty() || !seen.insert(item.first).second)
     throw std::runtime_error("Ambiguous Vanity claim receipt");
 }
 const auto canonicalKey = vanityPreview::canonical;
 if (fields.get<int>("version") != 1 || fields.get<std::string>("server") != server ||
     fields.get<std::string>("assignment_id") != state.assignmentId ||
     fields.get<std::string>("client_id") != state.clientId ||
     fields.get<int>("puzzle") != state.puzzle || fields.get<int>("range_id") != state.rangeId ||
     canonicalKey(fields.get<std::string>("start")) != canonicalKey(state.start) ||
     canonicalKey(fields.get<std::string>("end")) != canonicalKey(state.end) ||
     fields.get<std::string>("target") != state.target || fields.get<std::string>("engine") != "vanity" ||
     fields.get<std::string>("backend") != "cuda" || fields.get<std::string>("search_mode") != "linear")
   throw std::runtime_error("Vanity assignment differs from the protected claim receipt");
}
}
WindowsRunSession::WindowsRunSession(WindowsRunDependencies d):dependencies_(std::move(d)){}
fs::path WindowsRunSession::pendingClaimPath(const std::string& slot){
 if(!ExecutionSlot::valid(slot))throw std::invalid_argument("Invalid execution slot");
 return localPaths::dataDirectory()/"pending-claims"/pathFromUtf8(slot+".json");
}
int WindowsRunSession::run(const std::vector<std::string>& arguments,std::ostream& output) {
 try { return runPrepared(RunCommandOptions::parse(arguments),output); }
 catch(const std::exception& error){output<<"Windows run refused: "<<error.what()<<'\n';return 1;}
}
int WindowsRunSession::runPrepared(RunCommandOptions options,std::ostream& output) {
 try {
  if(options.command=="help") {output<<"Windows run controls: run [puzzle] [--once|--dry-run|--preflight-only] [--engine bitcrack|keyhunt] [--backend cuda|opencl|cpu] [--device N] [--threads N] [--duration-minutes 1..360] [--server URL]\nstatus | stop | safestop | version\n";return 0;}
  if(options.command=="version"){output<<"OpenPuzzle "<<windowsClientVersion<<" Windows\n";return 0;}
  const auto slot=Store::executionSlot();const auto pending=pendingClaimPath(slot);
  if(options.command=="status") {
    const auto identity=Control::runtimeIdentityStatus(slot);const auto state=client::ExecutionSyncService().inspect(slot);
    output<<"Slot: "<<slot<<"\nRuntime: "<<(identity==Control::RuntimeIdentityStatus::Running?"running":identity==Control::RuntimeIdentityStatus::Unavailable?"unknown":"idle")
      <<"\nAssignment: "<<(state.identityUnavailable?"identity unavailable":state.hasState?state.state.assignmentId:present(Store::path(slot))?"unreadable state preserved":"none")
      <<"\nPending claim: "<<(present(pending)?"preserved":"none")<<'\n';return 0;
  }
  if(options.command=="stop"||options.command=="safestop") {
    const bool requested=options.command=="stop"?Control::requestStop(slot):Control::requestSafeStop(slot);
    output<<(requested?"Cooperative request recorded":"No verified runtime accepted the request")<<'\n';return requested?0:1;
  }
  if(!options.resumeVanity&&(present(Store::path(slot))||present(pending))){output<<"Pending assignment/claim preserved; resolve it before requesting work\n";return 1;}
  if(Control::runtimeIdentityStatus(slot)!=Control::RuntimeIdentityStatus::Inactive){output<<"Execution slot is occupied or has an unavailable identity\n";return 1;}
  if(!dependencies_.preflight||!dependencies_.capabilities)throw std::invalid_argument("Verified Windows capability provider and preflight are required");
  std::optional<PuzzleExecutionMetadata> metadata;
  if(options.puzzle){if(!dependencies_.metadata||(metadata=dependencies_.metadata(options.puzzle))==std::nullopt||metadata->puzzle!=options.puzzle)throw std::invalid_argument("Requested puzzle metadata is unavailable");
    if(metadata->searchMode!="linear")throw std::invalid_argument("Windows Kangaroo planning/checkpoint recovery remains pending");}
  if(options.engine=="kangaroo")throw std::invalid_argument("Windows Kangaroo planning/checkpoint recovery remains pending");
  if(options.engine=="vanity"&&!metadata)throw std::invalid_argument("Windows Vanity requires an explicit puzzle");
  // Select only an explicitly verified binding; no fallback after claiming work.
  const auto chosen=std::find_if(dependencies_.engines.begin(),dependencies_.engines.end(),[&](const auto& e){
    return e.verified&&(e.engine=="bitcrack"||e.engine=="keyhunt"||(e.engine=="vanity"&&options.engine=="vanity"))&&
      (options.engine.empty()||e.engine==options.engine)&&(options.backend.empty()||e.backend==options.backend)&&
      (e.engine=="bitcrack"?(e.backend=="cuda"||e.backend=="opencl"):e.engine=="vanity"?e.backend=="cuda":e.backend=="cpu")&&
      (!metadata||metadata->requiredBackend.empty()||metadata->requiredBackend==e.backend);
  });
  if(chosen==dependencies_.engines.end())throw std::invalid_argument("No verified engine/backend matches this request");
  options.engine=chosen->engine;options.backend=chosen->backend;
  if(options.backend=="cpu"&&options.gpuOptionsSpecified)throw std::invalid_argument("GPU launch options do not apply to the selected CPU engine");
  if(options.backend=="cpu"&&!options.threadsSpecified)options.threads=1;
  if(!chosen->executable.is_absolute()||!windowsFile::safePath(chosen->executable)||!fs::is_regular_file(chosen->executable)||
      !client::HttpRequestPolicy::validSpeed(chosen->speedMKeys))throw std::invalid_argument("Invalid verified engine binding");
  if(options.backend!="cpu"&&std::find(chosen->devices.begin(),chosen->devices.end(),options.device)==chosen->devices.end())throw std::invalid_argument("Requested device is absent from the verified inventory");
  if(!dependencies_.preflight(options,*chosen))throw std::runtime_error("Windows engine/hardware/profile preflight refused this run");
  output<<"Engine: "<<options.engine<<"\nBackend: "<<options.backend<<"\nDevice: "<<options.device<<"\nTarget duration: "<<options.durationMinutes<<" minutes\n";
  if(options.dryRun||options.preflightOnly){output<<"Local preflight only; no client identity, registration, heartbeat, claim or search launch\n";return 0;}
  const auto server=options.server.empty()?dependencies_.serverUrl:options.server;
  if(!client::HttpRequestPolicy::endpoint(server))throw std::invalid_argument("Invalid coordination URL");
  const auto clientId=client::ClientIdentity::loadOrCreate();if(clientId.empty())throw std::runtime_error("Stable Windows client identity unavailable");
  auto caps=dependencies_.capabilities(clientId);
  const bool available=std::any_of(caps.engines.begin(),caps.engines.end(),[&](const auto& e){return e.installed&&e.available&&canonical(e.name)==options.engine&&canonical(e.backend)==options.backend;});
  if(caps.clientId!=clientId||!caps.valid()||canonical(caps.platform)!="windows"||caps.status!="idle"||!available||
      (options.backend!="cpu"&&!std::any_of(caps.gpus.begin(),caps.gpus.end(),[&](const auto& g){return g.valid()&&canonical(g.backend)==options.backend;})))throw std::invalid_argument("Binding differs from verified Windows heartbeat capabilities");
  if(options.backend=="cpu"&&options.threads>caps.cpu.threads)throw std::invalid_argument("Requested CPU threads exceed verified inventory");
  WindowsRuntimeSession runtime(server,caps,[&]{
    return (options.resumeVanity || (!present(pending) && !present(Store::path(slot)))) &&
      dependencies_.preflight(options,*chosen);
  },dependencies_.thermal);
  bool registered=false;
  return runtime.runContinuous([&](WindowsRuntimeSession& owner){
    if(!owner.thermalReady())return ClientIterationResult(2);
    // Runtime ownership serializes requests; a separate lock also protects receipts.
    WorkspaceSecurity::prepare(pending.parent_path());windowsFile::Lock lock(pending.wstring()+L".lock");
    if(!lock.locked())throw std::runtime_error("Pending claim publication is occupied; existing data preserved");
    client::HttpRangeClient http(server);
    if(Control::stopRequested(slot)||Control::safeStopRequested(slot))return ClientIterationResult(1);
    if (options.resumeVanity) {
      const auto observed = client::ExecutionSyncService().inspect(slot);
      auto state = observed.state;
      const auto previous = windowsExecutionRecord(state);
      if (!observed.hasState || observed.running || observed.identityUnavailable || observed.solutionFound ||
          !previous || state.engine != "Vanity" || state.clientId != clientId || state.puzzle != options.puzzle ||
          state.target != metadata->address || state.device != options.device || state.blocks != options.blocks ||
          (!options.expectedAssignment.empty() && state.assignmentId != options.expectedAssignment) ||
          !RunAssignmentPlan::safeAssignmentId(state.assignmentId))
        throw std::runtime_error("No matching stopped Vanity assignment to resume");
      const auto workspace = localPaths::dataDirectory() / "assignments" / pathFromUtf8(state.assignmentId);
      if (workspace != pathFromUtf8(state.workspace))
        throw std::runtime_error("Vanity workspace differs from assignment identity");
      bool absent = false;
      const auto saved = windowsFile::readPrivate(pending, absent, 16384);
      if (!saved) throw std::runtime_error("Vanity claim receipt is unavailable");
      verifyVanityReceipt(*saved, state, server);
      const auto targets = windowsFile::readPrivate(workspace/"targets.txt", absent, 8192);
      if (!targets || *targets != "OPV-TARGETS-1\np2pkh " + state.target + "\n")
        throw std::runtime_error("Vanity target file differs from the assignment");
      const auto proof = readWindowsVanityEvidence(state, WindowsEngineProcess::generationPath(workspace, *previous));
      if (!proof || proof->solution)
        throw std::runtime_error("Vanity checkpoint requires review");
      if (proof->complete && observed.nativeExitCode == 0) {
        const auto completed = client::ExecutionSyncService().tick(server, slot);
        if (!completed.completionUploaded || !completed.stateRemoved)
          throw std::runtime_error("Vanity completion remains pending; report preserved");
      } else {
        const auto previousResult = windowsVanityResult(state);
        const auto outputData = windowsFile::readPrivate(previousResult, absent, 4096);
        if ((!outputData && !absent) || (outputData && !outputData->empty()))
          throw std::runtime_error("Native Vanity result requires review before resume");
        state.vanityResumeOffset += proof->checked;
        (void)windowsVanityPlan(state);
        if (!http.progress(state.assignmentId, state.clientId, chosen->speedMKeys, std::to_string(state.vanityResumeOffset)))
          throw std::runtime_error("Vanity lease refused; checkpoint preserved: " + http.lastError());
        if (outputData && previousResult == windowsVanityResult(state)) {
          const auto archive = workspace / ("vanity-empty-" + previous->generation + ".txt");
          if (!MoveFileExW(previousResult.c_str(), archive.c_str(), 0))
            throw std::runtime_error("Cannot archive previous empty Vanity result");
        }
        output << "Resuming Vanity assignment: " << state.assignmentId << " from " << state.vanityResumeOffset << " keys\n";
        const auto result = owner.execute(chosen->executable, {}, state, previous);
        if (result.status != ClientIterationStatus::Completed || result.exitCode != 0 || present(Store::path(slot))) return result;
      }
      std::error_code error;
      if (!fs::remove(pending, error) || error) throw std::runtime_error("Vanity receipt cleanup failed");
      return ClientIterationResult(0);
    }
    if(present(pending))throw std::runtime_error("Pending claim publication is occupied; existing data preserved");
    // Retain an intent before the network call: a lost response is not proof that
    // the server did not allocate work. Ambiguous failures never trigger a new claim.
    const auto intent="{\"version\":1,\"status\":\"requesting\",\"client_id\":\""+escapeJsonString(clientId)+"\",\"server\":\""+escapeJsonString(server)+"\",\"puzzle\":"+std::to_string(options.puzzle)+"}\n";
    if(!writePrivateFileAtomically(pending,intent))throw std::runtime_error("Claim intent publication failed; no assignment requested");
    const auto requested = options.engine=="vanity" ? static_cast<std::uint64_t>(
        std::clamp(chosen->speedMKeys * options.durationMinutes * 60000000.0, 1.0,
                   static_cast<double>(vanityPreview::NativeRangeLimit))) : 0;
    const auto claimed=http.claimResult(clientId,options.puzzle,options.durationMinutes,chosen->speedMKeys,options.backend,"linear",requested);
    if(claimed.unavailable()){
      std::error_code error;if(!fs::remove(pending,error)||error)throw std::runtime_error("Unavailable claim intent could not be removed");
      return ClientIterationResult::unavailable(claimed.message);
    }
    if(!claimed.assigned()||!claimed.assignment)throw std::runtime_error("Claim response is uncertain; intent preserved and further claims blocked: "+claimed.message);
    const auto assignment=*claimed.assignment;
    if(!writePrivateFileAtomically(pending,receipt(assignment,clientId,server,options)))throw std::runtime_error("Unable to persist claimed assignment; launch refused");
    const auto workspace=localPaths::dataDirectory()/"assignments"/pathFromUtf8(assignment.assignmentId);
    const auto plan=RunAssignmentPlan::build(options,assignment,clientId,workspace,metadata?&*metadata:nullptr);
    if(!dependencies_.preflight(options,*chosen))throw std::runtime_error("Engine/profile changed after claim; receipt preserved and launch refused");
    if(present(workspace))throw std::runtime_error("Assignment workspace already exists; previous contents preserved");
    WorkspaceSecurity::prepare(workspace);
    if(!plan.targetFileContent.empty()&&!writePrivateFileAtomically(workspace/"targets.txt",plan.targetFileContent))throw std::runtime_error("Unable to publish private engine targets");
    auto state=plan.state;state.command=WindowsCommandLine::build(pathUtf8(chosen->executable),plan.arguments);
    output<<"Assignment: "<<assignment.assignmentId<<"\nPuzzle: "<<assignment.puzzle<<"\n";
    const auto result=owner.execute(chosen->executable,plan.arguments,std::move(state));
    // Only an acknowledged successful terminal result can discard its receipt.
    if(result.status==ClientIterationStatus::Completed&&result.exitCode==0&&!present(Store::path(slot))){std::error_code error;if(!fs::remove(pending,error)||error)throw std::runtime_error("Completed claim receipt could not be removed");}
    return result;
  },options.once||options.resumeVanity,[&]{
    if(registered)return ClientIterationResult(0);
    client::HttpRangeClient http(server);
    if(!http.registerClient({clientId}))return ClientIterationResult::retry(http.lastError());
    registered=true;return ClientIterationResult(0);
  },options.resumeVanity);
 }catch(const std::exception& error){output<<"Windows run refused: "<<error.what()<<"\nPending local data preserved\n";return 1;}
}
}
