#include "openpuzzle/runtime/WindowsRuntimeSession.hpp"
#include "openpuzzle/runtime/WindowsExecutionLauncher.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/HttpRequestPolicy.hpp"
#include "openpuzzle/core/SignalHandler.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <thread>
namespace openpuzzle {
using Store=client::ClientStateStore;
using Control=ClientRuntimeControl;
using Sync=client::ExecutionSyncService;
namespace {
std::string canonical(std::string value) {
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
  return value;
}
ClientIterationResult failure(const char* message) {
  ClientIterationResult result(1);result.message=message;return result;
}
}
WindowsRuntimeSession::WindowsRuntimeSession(std::string serverUrl,client::ClientHeartbeat capabilities,
    std::function<bool()> preflight):serverUrl_(std::move(serverUrl)),slot_(Store::executionSlot()),
    capabilities_(std::move(capabilities)),preflight_(std::move(preflight)) {
  // Validate before slot acquisition, child creation or network calls.
  if (!preflight_ || !capabilities_.valid() || canonical(capabilities_.platform)!="windows" ||
      capabilities_.status!="idle" || !capabilities_.activeEngine.empty() || !capabilities_.activeBackend.empty() ||
      !capabilities_.activeBackends.empty()) throw std::invalid_argument("Verified idle Windows capabilities and preflight are required");
  if (!client::HttpRequestPolicy::endpoint(serverUrl_)) throw std::invalid_argument("Invalid Windows session server URL");
}
bool WindowsRuntimeSession::hasState()const {
  std::error_code error;
  const auto status=std::filesystem::symlink_status(Store::path(slot_),error);
  return (error && error!=std::errc::no_such_file_or_directory) || std::filesystem::exists(status);
}
bool WindowsRuntimeSession::stopped()const {
  return SignalHandler::stopRequested() || Control::stopRequested(slot_);
}
void WindowsRuntimeSession::poll() {
  if (engine_ && !completed_ && engine_->wait(0).has_value()) {completed_=true;wakePending_=true;}
}
bool WindowsRuntimeSession::stopOwned(const std::string& workspace) {
  if (!engine_ || workspace!=state_.workspace || Store::executionSlot()!=slot_) return false;
  try { engine_->stop(125);completed_=true;return true; } catch (...) { return false; }
}
void WindowsRuntimeSession::closeOwned()noexcept {
  if (engine_) {
    try { poll();if(!completed_) engine_->stop(125); }catch (...) {}
    engine_.reset();completed_=false;
  }
}
WindowsRuntimeSession::~WindowsRuntimeSession(){closeOwned();}
ClientRuntimeDependencies WindowsRuntimeSession::dependencies() {
  ClientRuntimeDependencies d;
  d.sync=[this](const std::string& url){
    poll();wakePending_=false;auto result=Sync().tick(url,slot_);
    awaitingProgress_=result.running && !result.hasProgress;return result;
  };
  d.heartbeat=[this](const std::string& url){
    client::ClientHeartbeatResult result;result.heartbeat=capabilities_;
    poll();const auto inspected=Sync().inspect(slot_);
    if (hasState() && (!inspected.hasState || inspected.identityUnavailable)) {
      result.error="Pending Windows execution identity is unavailable";return result;
    }
    if (inspected.running) {
      result.heartbeat.status="running";
      result.heartbeat.activeEngine=inspected.state.engine;
      result.heartbeat.activeBackend=inspected.state.backend;
      result.heartbeat.activeBackends={inspected.state.backend};
    }
    client::HttpRangeClient http(url);result.success=http.heartbeat(result.heartbeat);result.error=http.lastError();return result;
  };
  d.stopExecution=[this](const std::string& workspace){return stopOwned(workspace);};
  d.reportSolution=[this](const std::string& url,const std::string& assignment,const std::string& clientId,std::string& error){
    const auto local=Sync().inspect(slot_);
    if(!local.hasState || local.identityUnavailable || !local.solutionFound ||
       assignment!=state_.assignmentId || clientId!=state_.clientId || !engine_ ||
       windowsExecutionRecord(local.state)!=engine_->identity()){error="Local solution identity is unavailable";return false;}
    client::HttpRangeClient http(url);auto ok=http.reportSolution(assignment,clientId);error=http.lastError();return ok;
  };
  d.finalizeAssignment=[this](const std::string& url,const std::string& assignment,const std::string& clientId,int exit,
      const std::string& status,const std::string& keys,std::string& error){
    const auto local=Sync().inspect(slot_);
    if(!local.hasState || local.identityUnavailable || assignment!=state_.assignmentId ||
       clientId!=state_.clientId || !engine_ || windowsExecutionRecord(local.state)!=engine_->identity()){
      error="Local assignment identity is unavailable; finalization preserved";return client::AssignmentUploadStatus::TemporaryFailure;
    }
    client::HttpRangeClient http(url);
    if(http.complete(assignment,clientId,exit,status,keys))return client::AssignmentUploadStatus::Uploaded;
    error=http.lastError();return Sync::classifyCompletionError(http.lastErrorCode());
  };
  d.finalKeysChecked=[this](const std::string& workspace){
    if(!engine_ || workspace!=state_.workspace)return std::string{};
    const auto generation=WindowsEngineProcess::generationPath(pathFromUtf8(workspace),engine_->identity());
    const auto progress=Sync::latestProgress(pathUtf8(generation),state_.engine);
    return progress?progress->keysChecked:std::string{};
  };
  d.removeState=[this]{
    windowsFile::Lock lock(Store::path(slot_).wstring()+L".execution.lock");
    const auto stored=Store::load(slot_);
    if(!lock.locked() || !stored || !engine_ || windowsExecutionRecord(*stored)!=engine_->identity() ||
        stored->assignmentId!=state_.assignmentId || stored->clientId!=state_.clientId)return false;
    return Store::remove(slot_);
  };
  d.hasState=[this]{return hasState();};
  d.acquireRuntime=[this]{return Store::executionSlot()==slot_ && Control::acquire();};
  d.releaseRuntime=[this]{closeOwned();(void)Control::release();};
  d.stopRequested=[this]{return stopped();};
  d.safeStopRequested=[this]{return Control::safeStopRequested(slot_);};
  d.clearSafeStop=[this]{return Control::clearSafeStop(slot_);};
  d.prepareSignals=[] {SignalHandler::reset();SignalHandler::install();};
  d.sleep=[this](std::chrono::seconds duration){
    const auto deadline=std::chrono::steady_clock::now()+duration;
    do {poll();if(wakePending_ || stopped())return;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }while(std::chrono::steady_clock::now()<deadline);
  };
  d.wakeRequested=[this]{
    poll();if(wakePending_){wakePending_=false;return true;}
    if(engine_) {
      const auto observed=Sync().inspect(slot_);
      if(!solutionWakeConsumed_ && observed.solutionFound){solutionWakeConsumed_=true;return true;}
      if(awaitingProgress_ && observed.hasProgress){awaitingProgress_=false;return true;}
    }
    return false;
  };
  return d;
}
int WindowsRuntimeSession::runContinuous(
    const std::function<ClientIterationResult(WindowsRuntimeSession&)>& prepare, bool once,
    const std::function<ClientIterationResult()>& initialize) {
  if(!prepare || !preflight_())return 1;
  ClientRuntime runtime(dependencies());bool initialHeartbeat=true;
  return runtime.runContinuous(serverUrl_,[&]{
    if(hasState())return failure("Pending or unreadable assignment preserved; synchronize it before requesting work");
    if(initialHeartbeat){
      if(initialize){const auto initialized=initialize();
        if(initialized.status!=ClientIterationStatus::Completed)
          return once?failure("Single-assignment initialization failed"):initialized;
      }
      const auto heartbeat=dependencies().heartbeat(serverUrl_);
      if(!heartbeat.success)return once?failure("Single-assignment idle heartbeat failed"):ClientIterationResult::retry(heartbeat.error);
      initialHeartbeat=false;
    }
    auto result=prepare(*this);
    if(once) {
      if(result.status==ClientIterationStatus::Retry)return failure("Single assignment request failed; pending state preserved");
      if(result.status==ClientIterationStatus::Unavailable)result=ClientIterationResult(0);
      if(result.status==ClientIterationStatus::Completed && !Control::requestSafeStop(slot_))
        return failure("Unable to finish the single-assignment runtime safely");
    }
    return result;
  });
}
ClientIterationResult WindowsRuntimeSession::execute(const std::filesystem::path& executable,
    const std::vector<std::string>& arguments,client::ClientExecutionState state) {
  if(Store::executionSlot()!=slot_ || engine_ || stopped() || Control::safeStopRequested(slot_) ||
      state.clientId!=capabilities_.clientId || hasState())return failure("Windows assignment launch blocked; existing state preserved");
  const auto backend=canonical(state.backend),name=canonical(state.engine);
  const bool available=std::any_of(capabilities_.engines.begin(),capabilities_.engines.end(),[&](const auto& e){
    return e.installed && e.available && canonical(e.name)==name && canonical(e.backend)==backend;
  });
  if(!available || (backend!="cpu" && !std::any_of(capabilities_.gpus.begin(),capabilities_.gpus.end(),[&](const auto& g){return canonical(g.backend)==backend && g.valid();})))
    return failure("Assignment engine/backend is not among verified Windows capabilities");
  state_=state;completed_=false;wakePending_=false;solutionWakeConsumed_=false;
  engine_=startWindowsExecution(executable,arguments,std::move(state));
  const int code=ClientRuntime(dependencies()).run(serverUrl_,state_.assignmentId,state_.clientId,state_.workspace);
  closeOwned();
  if(code==ClientRuntime::SolutionFoundExitCode)return ClientIterationResult::solutionFound();
  return ClientIterationResult(code);
}
}
