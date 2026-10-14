#include "WindowsRunRequestFixture.hpp"
#include "openpuzzle/runtime/WindowsAssignmentRecovery.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WindowsExecutionLauncher.hpp"
#include "openpuzzle/runtime/WindowsRunSession.hpp"
#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/ui/WindowsUiModel.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
using namespace openpuzzle;namespace fs=std::filesystem;using Store=client::ClientStateStore;using Control=ClientRuntimeControl;
std::string read(const fs::path& p){std::ifstream in(p);return {std::istreambuf_iterator<char>(in),{}};}
bool endpoint(const std::string& r,const char* p){return r.starts_with(std::string("POST ")+p+' ');}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Engine fixture required");Winsock sockets;const auto root=fs::temp_directory_path()/pathFromUtf8("op-recovery-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directory(root);
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())&&SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","primary"),"Recovery environment failed");
 const auto client=client::ClientIdentity::loadOrCreate();const auto pending=WindowsRunSession::pendingClaimPath("primary");WorkspaceSecurity::prepare(pending.parent_path());
 std::ostringstream output;
 const auto publish=[&](const std::string& text){require(writePrivateFileAtomically(WindowsRunSession::pendingClaimPath(Store::executionSlot()),text),"Receipt fixture write failed");};
 publish("{\"version\":1,\"status\":\"requesting\"}");const auto intent=read(pending);
 require(recoverWindowsAssignment("primary","http://127.0.0.1:1",output)==1&&read(pending)==intent&&!Control::running("primary"),"Uncertain claim was discarded or retried");fs::remove(pending);
 auto launch=[&](const std::string& id,const std::string& url,const std::string& mode,const std::string& backend="cuda"){
  client::ClientExecutionState s;s.active=true;s.assignmentId=id;s.clientId=client;s.puzzle=20;s.rangeId=1;s.start="1";s.end="10";s.engine="BitCrack";s.backend=backend;s.target="fixture-address";s.workspace=pathUtf8(localPaths::dataDirectory()/"assignments"/id);
  const auto work=pathFromUtf8(s.workspace);WorkspaceSecurity::prepare(work);const auto gate=work/"gate";require(writePrivateFileAtomically(gate,"ready"),"Gate publication failed");
  require(Control::acquire(),"Fixture slot acquisition failed");
  auto process=startWindowsExecution(fs::absolute(argv[1]),mode=="sleep"?std::vector<std::string>{"sleep"}:std::vector<std::string>{"execution",pathUtf8(gate),pathUtf8(work/"ready"),mode},s);
  const auto record="{\"version\":1,\"server\":\""+url+"\",\"client_id\":\""+client+"\",\"assignment_id\":\""+id+"\",\"puzzle\":20,\"range_id\":1,\"start\":\"1\",\"end\":\"10\",\"target\":\"fixture-address\",\"backend\":\""+backend+"\",\"search_mode\":\"linear\",\"engine\":\"bitcrack\"}";
  publish(record);return process;
 };
 {int completed=0;std::atomic_bool fail{true};RunServer server([&](const auto& r){require(endpoint(r,"/api/range/complete"),"Recovery registered or requested new work");++completed;require(r.find("\"keys_checked\":\"16\"")!=std::string::npos,"Recovered exact coverage mismatch");return std::string("{}");},[&](const auto&){return fail?std::string("503 Service Unavailable"):std::string("200 OK");});
  auto child=launch("completed",server.url(),"proof");require(child->wait(30000)==0,"Completion fixture failed");child.reset();require(Control::release(),"Owner fixture release failed");const auto receipt=read(pending),stored=read(Store::path("primary"));
  require(recoverWindowsAssignment("primary",server.url(),output)==1&&read(pending)==receipt&&read(Store::path("primary"))==stored,"Network failure discarded receipt/state");
  fail=false;require(recoverWindowsAssignment("primary",server.url(),output)==0&&!fs::exists(pending)&&!Store::load("primary"),"Acknowledged recovery retained or lost wrong state");server.finish();require(completed==2,"Recovery repeated completion unexpectedly");
 }
 {RunServer server([](const auto&){throw std::runtime_error("Unknown generation reached HTTP");return std::string("{}");});auto child=launch("uncertain",server.url(),"proof");require(child->wait(30000)==0,"Uncertain fixture failed");child.reset();Control::release();auto state=Store::load("primary");require(state.has_value(),"State missing");state->processGeneration="unverified-generation";require(Store::save(*state,"primary"),"Unknown state publication failed");
  const auto receipt=read(pending),stored=read(Store::path("primary"));require(recoverWindowsAssignment("primary",server.url(),output)==1&&read(pending)==receipt&&read(Store::path("primary"))==stored,"Unknown generation was discarded");server.finish();Store::remove("primary");fs::remove(pending);
 }
 {int notified=0;RunServer server([&](const auto& r){require(endpoint(r,"/api/puzzle/report-solution")&&r.find("private_key")==std::string::npos,"Solution recovery leaked local material or finalized");++notified;return std::string("{}");});auto child=launch("solution",server.url(),"proof");require(child->wait(30000)==0,"Solution fixture failed");child.reset();Control::release();
  const auto work=localPaths::dataDirectory()/"assignments"/"solution";const std::string key="K"+std::string(51,'1');require(writePrivateFileAtomically(work/"found.txt","OPENPUZZLE_SOLUTION_V1\naddress=fixture-address\nprivate_key_wif="+key+"\ncompression=compressed\n"),"Solution fixture publication failed");
  const auto receipt=read(pending);require(recoverWindowsAssignment("primary",server.url(),output)==0&&Store::load("primary")&&read(pending)==receipt,"Recovered solution did not preserve local state");server.finish();require(notified==1&&fs::exists(localPaths::root()/"solutions"/"Puzzle-20"/"solution"/"wallet-import.txt"),"Recovered solution export/notification failed");Store::remove("primary");fs::remove(pending);
 }
 {auto child=launch("occupied","http://127.0.0.1:1","sleep");const auto receipt=read(pending);require(recoverWindowsAssignment("primary","",output)==1&&read(pending)==receipt&&!child->wait(0),"Recovery stole an active runtime or stopped its child");child->stop();child.reset();Control::release();Store::remove("primary");fs::remove(pending);}
 // A UI command must select the GPU slot while the caller remains on primary.
 // Direct recovery still refuses a mismatched environment without touching data.
 for(const std::string backend:{"cuda","opencl"}){
  const auto slot=backend+"-0",id=slot+"-ui-solution";int notified=0;
  RunServer server([&](const auto& r){require(endpoint(r,"/api/puzzle/report-solution")&&r.find("private_key")==std::string::npos,"UI recovery requested work or leaked a private key");++notified;return std::string("{}");});
  require(SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT",slot.c_str()),"GPU fixture slot selection failed");
  auto child=launch(id,server.url(),"proof",backend);require(child->wait(30000)==0,"GPU solution fixture failed");child.reset();require(Control::release(),"GPU fixture owner release failed");
  const auto work=localPaths::dataDirectory()/"assignments"/id;
  require(writePrivateFileAtomically(work/"found.txt","OPENPUZZLE_SOLUTION_V1\naddress=fixture-address\nprivate_key_wif=K"+std::string(51,'1')+"\ncompression=compressed\n"),"GPU solution publication failed");
  const auto gpuPending=WindowsRunSession::pendingClaimPath(slot),statePath=Store::path(slot);const auto receipt=read(gpuPending),state=read(statePath);
  require(SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","primary"),"Parent slot restoration failed");
  std::ostringstream refused;require(recoverWindowsAssignment(slot,server.url(),refused)==1&&refused.str().find("Recovery slot differs")!=std::string::npos&&read(gpuPending)==receipt&&read(statePath)==state&&notified==0,"Direct recovery crossed execution slots");
  ui::WindowsUiSelection selection;selection.slot=slot;selection.server=server.url();WindowsCliApplication app;
  std::ostringstream recovered;const auto code=app.run(ui::windowsUiCommand(ui::UiAction::Recover,selection),recovered);if(code!=0)std::cerr<<recovered.str();
  require(code==0&&Store::executionSlot()=="primary"&&Store::load(slot)&&read(gpuPending)==receipt&&!Store::load("primary")&&!fs::exists(pending),"UI recovery failed to select or restore the GPU slot");
  server.finish();require(notified==1&&fs::exists(localPaths::root()/"solutions"/"Puzzle-20"/id/"wallet-import.txt"),"UI GPU solution export/notification failed");
  require(Store::remove(slot),"GPU fixture state cleanup failed");fs::remove(gpuPending);
 }
 std::cout<<"WindowsAssignmentRecoveryTests passed: ambiguous claims, retries, exact completion, generation identity, protected solutions and occupied slots\n";return 0;
}catch(const std::exception& e){Control::release();std::cerr<<e.what()<<'\n';return 1;}}
