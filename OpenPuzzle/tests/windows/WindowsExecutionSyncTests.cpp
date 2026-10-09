#include "WindowsLocalHttpFixture.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/WindowsExecutionLauncher.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include <filesystem>
#include <iostream>
using namespace openpuzzle;
namespace fs=std::filesystem;
using Sync=client::ExecutionSyncService;
using Store=client::ClientStateStore;
using Control=ClientRuntimeControl;
void waitFile(const fs::path& path) {
  auto deadline=GetTickCount64()+4000;
  while(!fs::exists(path)&&GetTickCount64()<deadline)Sleep(10);
  require(fs::exists(path),"Fixture readiness timed out");
}
void publish(const fs::path& path,const std::string& value="fixture") {
  require(writePrivateFileAtomically(path,value),"Fixture file publication failed");
}
client::ClientExecutionState assignment(const fs::path& root,const std::string& id) {
  client::ClientExecutionState state;
  state.active=true;state.assignmentId=id;state.clientId="fixture-client";state.puzzle=20;
  state.rangeId=1;state.engine="BitCrack";state.backend="cuda";
  state.start="0";state.end="F";state.workspace=pathUtf8(root/pathFromUtf8(id));return state;
}
std::optional<std::uint64_t> unavailable(int){return std::nullopt;}
void env(const fs::path& root) {
  require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str()),"Local data fixture failed");
  require(SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","cuda-5"),"Slot fixture failed");
}
int wmain(int argc,wchar_t** argv) {
  fs::path root;
  try {
    if(argc==5&&std::wstring(argv[1])==L"--owner") {
      root=argv[2];env(root);require(Control::acquire(),"Owner fixture acquisition failed");
      auto state=assignment(root,"fixture-crash");
      auto process=startWindowsExecution(fs::path(argv[3]),
          {"execution",pathUtf8(root/"crash-gate"),pathUtf8(root/"crash-engine-ready"),"proof"},state);
      waitFile(root/"crash-engine-ready");publish(fs::path(argv[4]));Sleep(15000);return 0;
    }
    require(argc==2,"Engine fixture path required");
    Winsock sockets;
    const auto fixture=fs::absolute(argv[1]);
    root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle-sync-ç-中文-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));
    fs::create_directories(root);env(root);require(Control::acquire(),"Fixture slot acquisition failed");
    Sync sync;
    {
      auto state=assignment(root,"fixture-complete");
      auto engine=startWindowsExecution(fixture,{"execution",pathUtf8(root/"gate"),pathUtf8(root/"ready"),"proof"},state);
      waitFile(root/"ready");auto saved=Store::load("cuda-5");
      require(saved&&windowsExecutionRecord(*saved)==engine->identity()&&saved->ownerPid==static_cast<int>(GetCurrentProcessId()),"Assignment was not bound before execution");
      bool blocked=false;try{startWindowsExecution(fixture,{"touch",pathUtf8(root/"must-not-run")},assignment(root,"other"));}catch(...){blocked=true;}
      require(blocked&&!fs::exists(root/"must-not-run")&&!fs::exists(root/"other"),"Pending assignment was overwritten");
      auto inspected=sync.inspect("cuda-5");require(inspected.running&&inspected.hasProgress&&inspected.progress.keysChecked=="8"&&inspected.progress.speedMKeys==12.5,"Generation log was not parsed by the real service");
      auto unknown=Sync(unavailable).inspect("cuda-5");require(unknown.identityUnavailable&&!unknown.interrupted,"Unavailable FILETIME was treated as stopped");
      publish(pathFromUtf8(state.workspace)/"bitcrack.log","Reached end of keyspace\n99.0 MKey/s (999 total)");
      publish(pathFromUtf8(state.workspace)/"exit.code","0");
      auto mixed=*saved;mixed.processPlatform.clear();require(Store::save(mixed,"cuda-5"),"Mixed-identity fixture failed");
      unknown=sync.tick("file:///unused","cuda-5");require(unknown.identityUnavailable&&!unknown.stateRemoved&&Store::load("cuda-5"),"Foreign identity was synchronized");
      require(Store::save(*saved,"cuda-5"),"Restore fixture state failed");
      auto other=engine->identity();other.generation="another-generation";
      publish(WindowsEngineProcess::identityPath(pathFromUtf8(state.workspace)),other.encode());
      require(sync.inspect("cuda-5").identityUnavailable,"Mismatched generation accepted");
      publish(WindowsEngineProcess::identityPath(pathFromUtf8(state.workspace)),engine->identity().encode());
      {Server server;auto result=sync.tick(server.url(),"cuda-5");require(result.progressUploaded&&!result.stateRemoved,"Progress upload failed");server.path("POST","/api/range/progress");auto body=server.body();require(body.find("\"keys_checked\":\"8\"")!=std::string::npos&&body.find("999")==std::string::npos,"Stale root progress uploaded");}
      publish(root/"gate");
      auto deadline=GetTickCount64()+3000;
      while(WindowsProcessIdentity::status(engine->identity().process.pid,engine->identity().process.creationTime)==WindowsProcessIdentity::Status::Running&&GetTickCount64()<deadline)Sleep(10);
      unknown=sync.inspect("cuda-5");require(unknown.identityUnavailable&&!unknown.interrupted,"Owner still publishing completion became cancelled");
      require(engine->wait(2000)==0,"Fixture completion failed");
      {Server server("{}","503 Service Unavailable");auto result=sync.tick(server.url(),"cuda-5");require(!result.completionUploaded&&!result.stateRemoved&&Store::load("cuda-5"),"Temporary failure lost assignment");server.path("POST","/api/range/complete");require(server.body().find("\"keys_checked\":\"16\"")!=std::string::npos,"Exact range count missing");}
      {Server server;auto result=sync.tick(server.url(),"cuda-5");require(result.completionUploaded&&result.stateRemoved&&result.nativeExitCode==0,"Completion acknowledgement not synchronized");server.finish();}
      require(!sync.tick("file:///unused","cuda-5").hasState,"Acknowledged completion repeated");
    }
    {
      auto state=assignment(root,"fixture-no-proof");
      auto engine=startWindowsExecution(fixture,{"execution",pathUtf8(root/"no-proof-gate"),pathUtf8(root/"no-proof-ready"),"none"},state);
      waitFile(root/"no-proof-ready");publish(pathFromUtf8(state.workspace)/"bitcrack.log","Reached end of keyspace");publish(pathFromUtf8(state.workspace)/"exit.code","0");publish(root/"no-proof-gate");require(engine->wait(2000)==0,"No-proof fixture failed");
      auto result=sync.tick("file:///unused","cuda-5");require(result.completionStatus==client::AssignmentUploadStatus::PermanentFailure&&!result.completionUploaded&&Store::load("cuda-5"),"Stale legacy proof accepted");require(Store::remove("cuda-5"),"Fixture state cleanup failed");
    }
    {
      auto state=assignment(root,"fixture-keyhunt");state.engine="KeyHunt";
      auto engine=startWindowsExecution(fixture,{"execution",pathUtf8(root/"keyhunt-gate"),pathUtf8(root/"keyhunt-ready"),"keyhunt"},state);
      waitFile(root/"keyhunt-ready");publish(root/"keyhunt-gate");require(engine->wait(2000)==0,"KeyHunt fixture failed");
      {Server server;auto result=sync.tick(server.url(),"cuda-5");require(result.completionUploaded&&result.stateRemoved,"Exact KeyHunt End proof was not accepted");require(server.body().find("\"keys_checked\":\"16\"")!=std::string::npos,"KeyHunt exact coverage missing");}
    }
    {
      auto state=assignment(root,"fixture-kangaroo");state.engine="Kangaroo";
      auto engine=startWindowsExecution(fixture,{"execution",pathUtf8(root/"kangaroo-gate"),pathUtf8(root/"kangaroo-ready"),"proof"},state);
      waitFile(root/"kangaroo-ready");publish(root/"kangaroo-gate");require(engine->wait(2000)==0,"Kangaroo fixture failed");
      auto result=sync.tick("file:///unused","cuda-5");require(result.completionStatus==client::AssignmentUploadStatus::PermanentFailure&&!result.completionUploaded&&Store::load("cuda-5"),"Probabilistic engine acquired deterministic coverage");require(Store::remove("cuda-5"),"Kangaroo fixture cleanup failed");
    }
    {
      auto state=assignment(root,"fixture-solution");
      auto engine=startWindowsExecution(fixture,{"execution",pathUtf8(root/"solution-gate"),pathUtf8(root/"solution-ready"),"none"},state);
      waitFile(root/"solution-ready");auto found=pathFromUtf8(state.workspace)/"found.txt";publish(found,"LOCAL-FIXTURE-MATERIAL-DO-NOT-SEND");
      auto result=sync.tick("file:///unused","cuda-5");require(result.solutionFound&&result.solutionPath==pathUtf8(found)&&!result.completionUploaded&&!result.progressUploaded&&Store::load("cuda-5"),"Solution content was handled by network sync");
      engine->stop();require(Store::remove("cuda-5"),"Solution fixture cleanup failed");
      bool blocked=false;try{startWindowsExecution(fixture,{"touch",pathUtf8(root/"must-not-run")},state);}catch(...){blocked=true;}require(blocked&&!fs::exists(root/"must-not-run"),"Preserved solution workspace was reused");
    }
    {
      auto state=assignment(root,"fixture-large-exit");
      auto engine=startWindowsExecution(fixture,{"execution",pathUtf8(root/"large-gate"),pathUtf8(root/"large-ready"),"large"},state);
      waitFile(root/"large-ready");publish(root/"large-gate");require(engine->wait(2000)==0x80000000UL,"DWORD exit was truncated");
      auto result=sync.inspect("cuda-5");require(result.nativeExitCode==0x80000000UL&&result.exitCode==-4,"Native exit not retained in sync result");
      {Server server;result=sync.tick(server.url(),"cuda-5");require(result.completionUploaded&&result.stateRemoved,"Large exit failure was not synchronized");require(server.body().find("\"exit_code\":-4")!=std::string::npos&&server.body().find("\"status\":\"failed\"")!=std::string::npos,"Large exit became success");}
    }
    {
      bool refused=false;try{WindowsEngineProcess::start(fixture,{"touch",pathUtf8(root/"never-resumed")},root/"publication-failure",[](const WindowsEngineRecord&){return false;});}catch(...){refused=true;}
      require(refused&&!fs::exists(root/"never-resumed"),"Child ran before assignment publication succeeded");
    }
    require(Control::release(),"Parent fixture release failed");
    {
      auto owner=WindowsEngineProcess::start(fs::absolute(argv[0]),{"--owner",pathUtf8(root),pathUtf8(fixture),pathUtf8(root/"owner-ready")},root/"owner-workspace");
      waitFile(root/"owner-ready");require(Store::load("cuda-5").has_value(),"Crash fixture did not persist assignment");owner->stop(125);
      require(Control::acquire(),"Stale owner slot was not recovered");
      {Server server;auto result=sync.tick(server.url(),"cuda-5");require(result.interrupted&&result.exitCode==-3&&result.completionUploaded&&result.stateRemoved,"Owner crash was not synchronized as interruption");auto body=server.body();require(body.find("\"status\":\"cancelled\"")!=std::string::npos&&body.find("completed")==std::string::npos,"Owner crash became completed coverage");}
      require(Control::release(),"Recovered fixture release failed");
    }
    fs::remove_all(root);
    std::cout<<"WindowsExecutionSyncTests passed: prepared state, exact generations, native progress/completion, retries, solutions and owner crash\n";return 0;
  } catch(const std::exception& e) {
    Control::release();std::cerr<<e.what()<<'\n';return 1;
  }
}
