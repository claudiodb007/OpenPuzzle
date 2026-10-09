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
#include <algorithm>
#include <cctype>
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
}
WindowsRunSession::WindowsRunSession(WindowsRunDependencies d):dependencies_(std::move(d)){}
fs::path WindowsRunSession::pendingClaimPath(const std::string& slot){
 if(!ExecutionSlot::valid(slot))throw std::invalid_argument("Invalid execution slot");
 return localPaths::dataDirectory()/"pending-claims"/pathFromUtf8(slot+".json");
}
int WindowsRunSession::run(const std::vector<std::string>& arguments,std::ostream& output) {
 try {
  auto options=RunCommandOptions::parse(arguments);
  if(options.command=="help") {output<<"Windows run controls: run [puzzle] [--once|--dry-run|--preflight-only] [--engine bitcrack|keyhunt] [--backend cuda|opencl|cpu] [--device N] [--threads N] [--duration-minutes 1..360] [--server URL]\nstatus | stop | safestop | version\n";return 0;}
  if(options.command=="version"){output<<"OpenPuzzle Windows run foundation (Linux source release 1.0.46)\n";return 0;}
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
  if(present(Store::path(slot))||present(pending)){output<<"Pending assignment/claim preserved; resolve it before requesting work\n";return 1;}
  if(Control::runtimeIdentityStatus(slot)!=Control::RuntimeIdentityStatus::Inactive){output<<"Execution slot is occupied or has an unavailable identity\n";return 1;}
  if(!dependencies_.preflight||!dependencies_.capabilities)throw std::invalid_argument("Verified Windows capability provider and preflight are required");
  std::optional<PuzzleExecutionMetadata> metadata;
  if(options.puzzle){if(!dependencies_.metadata||(metadata=dependencies_.metadata(options.puzzle))==std::nullopt||metadata->puzzle!=options.puzzle)throw std::invalid_argument("Requested puzzle metadata is unavailable");
    if(metadata->searchMode!="linear")throw std::invalid_argument("Windows Kangaroo planning/checkpoint recovery remains pending");}
  if(options.engine=="kangaroo")throw std::invalid_argument("Windows Kangaroo planning/checkpoint recovery remains pending");
  // Select only an explicitly verified binding; no fallback after claiming work.
  const auto chosen=std::find_if(dependencies_.engines.begin(),dependencies_.engines.end(),[&](const auto& e){
    return e.verified&&(e.engine=="bitcrack"||e.engine=="keyhunt")&&
      (options.engine.empty()||e.engine==options.engine)&&(options.backend.empty()||e.backend==options.backend)&&
      (e.engine=="bitcrack"?(e.backend=="cuda"||e.backend=="opencl"):e.backend=="cpu")&&
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
  if(options.dryRun||options.preflightOnly){output<<"Local preflight only; no identity, registration, heartbeat, claim or engine launch\n";return 0;}
  const auto server=options.server.empty()?dependencies_.serverUrl:options.server;
  if(!client::HttpRequestPolicy::endpoint(server))throw std::invalid_argument("Invalid coordination URL");
  const auto clientId=client::ClientIdentity::loadOrCreate();if(clientId.empty())throw std::runtime_error("Stable Windows client identity unavailable");
  auto caps=dependencies_.capabilities(clientId);
  const bool available=std::any_of(caps.engines.begin(),caps.engines.end(),[&](const auto& e){return e.installed&&e.available&&canonical(e.name)==options.engine&&canonical(e.backend)==options.backend;});
  if(caps.clientId!=clientId||!caps.valid()||canonical(caps.platform)!="windows"||caps.status!="idle"||!available||
      (options.backend!="cpu"&&!std::any_of(caps.gpus.begin(),caps.gpus.end(),[&](const auto& g){return g.valid()&&canonical(g.backend)==options.backend;})))throw std::invalid_argument("Binding differs from verified Windows heartbeat capabilities");
  if(options.backend=="cpu"&&options.threads>caps.cpu.threads)throw std::invalid_argument("Requested CPU threads exceed verified inventory");
  WindowsRuntimeSession runtime(server,caps,[&]{return !present(pending)&&!present(Store::path(slot))&&dependencies_.preflight(options,*chosen);});
  bool registered=false;
  return runtime.runContinuous([&](WindowsRuntimeSession& owner){
    // Runtime ownership serializes requests; a separate lock also protects receipts.
    WorkspaceSecurity::prepare(pending.parent_path());windowsFile::Lock lock(pending.wstring()+L".lock");
    if(!lock.locked()||present(pending))throw std::runtime_error("Pending claim publication is occupied; existing data preserved");
    client::HttpRangeClient http(server);
    if(Control::stopRequested(slot)||Control::safeStopRequested(slot))return ClientIterationResult(1);
    // Retain an intent before the network call: a lost response is not proof that
    // the server did not allocate work. Ambiguous failures never trigger a new claim.
    const auto intent="{\"version\":1,\"status\":\"requesting\",\"client_id\":\""+escapeJsonString(clientId)+"\",\"server\":\""+escapeJsonString(server)+"\",\"puzzle\":"+std::to_string(options.puzzle)+"}\n";
    if(!writePrivateFileAtomically(pending,intent))throw std::runtime_error("Claim intent publication failed; no assignment requested");
    const auto claimed=http.claimResult(clientId,options.puzzle,options.durationMinutes,chosen->speedMKeys,options.backend,"linear");
    if(claimed.unavailable()){
      std::error_code error;if(!fs::remove(pending,error)||error)throw std::runtime_error("Unavailable claim intent could not be removed");
      return ClientIterationResult::unavailable(claimed.message);
    }
    if(!claimed.assigned()||!claimed.assignment)throw std::runtime_error("Claim response is uncertain; intent preserved and further claims blocked: "+claimed.message);
    const auto assignment=*claimed.assignment;
    if(!writePrivateFileAtomically(pending,receipt(assignment,clientId,server,options)))throw std::runtime_error("Unable to persist claimed assignment; launch refused");
    const auto workspace=localPaths::dataDirectory()/"assignments"/pathFromUtf8(assignment.assignmentId);
    const auto plan=RunAssignmentPlan::build(options,assignment,clientId,workspace,metadata?&*metadata:nullptr);
    if(present(workspace))throw std::runtime_error("Assignment workspace already exists; previous contents preserved");
    WorkspaceSecurity::prepare(workspace);
    if(!plan.targetFileContent.empty()&&!writePrivateFileAtomically(workspace/"targets.txt",plan.targetFileContent))throw std::runtime_error("Unable to publish private engine targets");
    auto state=plan.state;state.command=WindowsCommandLine::build(pathUtf8(chosen->executable),plan.arguments);
    output<<"Assignment: "<<assignment.assignmentId<<"\nPuzzle: "<<assignment.puzzle<<"\n";
    const auto result=owner.execute(chosen->executable,plan.arguments,std::move(state));
    // Only an acknowledged successful terminal result can discard its receipt.
    if(result.status==ClientIterationStatus::Completed&&result.exitCode==0&&!present(Store::path(slot))){std::error_code error;if(!fs::remove(pending,error)||error)throw std::runtime_error("Completed claim receipt could not be removed");}
    return result;
  },options.once,[&]{
    if(registered)return ClientIterationResult(0);
    client::HttpRangeClient http(server);
    if(!http.registerClient({clientId}))return ClientIterationResult::retry(http.lastError());
    registered=true;return ClientIterationResult(0);
  });
 }catch(const std::exception& error){output<<"Windows run refused: "<<error.what()<<"\nPending local data preserved\n";return 1;}
}
}
