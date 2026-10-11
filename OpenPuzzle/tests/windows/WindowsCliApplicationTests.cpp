#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace openpuzzle;namespace fs=std::filesystem;using Control=ClientRuntimeControl;using Store=client::ClientStateStore;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string read(const fs::path& p){std::ifstream input(p);std::ostringstream out;out<<input.rdbuf();return out.str();}
bool publish(const fs::path& p,const std::string& contents){WorkspaceSecurity::prepare(p.parent_path());return writePrivateFileAtomically(p,contents);}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Native CLI executable path required");
 const auto root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle-cli ç 中文-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));
 fs::create_directories(root);const auto profile=root/pathFromUtf8("profile ç 中文");
 require(SetEnvironmentVariableW(L"LOCALAPPDATA",profile.c_str())&&SetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",nullptr),"CLI test environment setup failed");
 const auto cli=root/pathFromUtf8("executáveis ç 中文 & literal.exe");require(!fs::exists(cli),"CLI fixture path already exists");fs::copy_file(fs::absolute(argv[1]),cli);
 WindowsCliApplication app({},[]{WindowsGpuInventory inventory;inventory.nativeCpuAvailable=true;inventory.logicalProcessors=8;inventory.systemMemoryMiB=16384;return inventory;});std::ostringstream output;
 require(app.run({},output)==0&&app.run({"version"},output)==0,"Default console information failed");
 require(!fs::exists(localPaths::root()),"Help or version created a profile");
 bool missing=false;require(!windowsFile::readPrivate(localPaths::root()/"not-created"/"state",missing)&&missing,"Missing parent directory is incorrectly treated as unavailable file");
 require(app.run({"status"},output)==0&&output.str().find("Runtime: idle")!=std::string::npos,"Fresh profile is incorrectly unknown");
 require(!fs::exists(localPaths::root()),"Status created a profile");
 require(app.run({"status","--all"},output)==0,"Fresh all-slot status failed");
 require(!fs::exists(localPaths::root()),"All-slot status created a profile");
 require(app.run({"doctor","--offline"},output)==0,"Fresh offline doctor failed");
 require(!fs::exists(localPaths::root()),"Local information created a profile or client identity");
 int child=0;
 auto execute=[&](const std::vector<std::string>& args,unsigned int code,const std::string& expected){
  auto process=WindowsEngineProcess::start(cli,args,root/("child-"+std::to_string(++child)));
  auto exit=process->wait(15000);if(!exit){process->stop(124);throw std::runtime_error("Native CLI exceeded its local command deadline");}
  const auto text=read(process->logPath());
  const bool unavailableHardware=args.front()=="doctor"&&*exit==2&&text.find("HARDWARE DIAGNOSTIC UNAVAILABLE")!=std::string::npos;
  if((*exit!=code&&!unavailableHardware)||text.find(expected)==std::string::npos)throw std::runtime_error("Native CLI result differs: exit="+std::to_string(*exit)+"; expected="+std::to_string(code)+"; output="+text);
 };
 execute({"version"},0,"OpenPuzzle 1.0.46 Windows CLI preview");
 execute({"paths","--slot","cuda-2"},0,"client-cuda-2.state");
 execute({"doctor","--offline"},0,"Server connection: SKIPPED");
 execute({"status","--all"},0,"Runtime: idle");
 execute({"run","--once","--device","0x"},1,"refused");
 execute({"run","--once","--slot","cuda-2"},1,"Run requires --profile");
 execute({"run",pathUtf8(fs::path(L"未知 ç & command"))},1,"refused");
 require(!fs::exists(localPaths::root()),"Native console startup registered, wrote identity or created local profile");
 for(const auto& bad:std::vector<std::vector<std::string>>{{"doctor","--online"},{"doctor","--offline","--offline"},{"version","extra"},{"paths","extra"},{"status","--unknown"},{"status","--slot","../outside"},{"stop","--slot","cuda-01"},{"run","--slot","cuda-1","--slot","cuda-2"},{""}})
  require(app.run(bad,output)==1,"Invalid console command accepted");
 require(SetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",L"invalid-slot"),"Invalid environment fixture failed");
 require(app.run({"status"},output)==1&&app.run({"status","--slot","cpu"},output)==0,"Invalid slot environment silently fell back or explicit selection failed");
 require(SetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",L"primary"),"Runtime test slot setup failed");
 require(app.run({"run","--once","--slot","cuda-2"},output)==1,"Missing Windows search provider claimed work");
 WindowsRunDependencies bindings;int localChecks=0;bindings.engines={{"bitcrack","cuda",cli,{2},12.5,true}};
 bindings.preflight=[&](const auto&,const auto&){++localChecks;require(Store::executionSlot()=="cuda-2","Run preflight selected another slot");return true;};
 bindings.capabilities=[](const std::string&)->client::ClientHeartbeat{throw std::runtime_error("Dry run reached heartbeat provider");};
 require(WindowsCliApplication(bindings).run({"run","--dry-run","--device","2","--slot","cuda-2"},output)==0&&localChecks==1,"Injected local preflight dispatch failed");
 char restored[64]{};GetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT",restored,sizeof(restored));
 require(std::string(restored)=="primary","Run dispatch did not restore the slot environment");
 require(Control::acquire(),"Local owner acquisition failed");
 require(app.run({"stop","--slot","cuda-2"},output)==1&&!Control::stopRequested("primary"),"Wrong-slot control affected the owner");
 require(app.run({"stop","--slot","primary"},output)==0&&Control::stopRequested("primary"),"Local stop did not reach verified owner");
 require(Control::clearStop("primary")&&app.run({"safestop"},output)==0&&Control::safeStopRequested("primary"),"Local safe stop did not reach verified owner");
 require(Control::release(),"Local runtime release failed");
 const auto config=pathFromUtf8(ConfigurationManager::configPath());
 require(publish(config,"not-json; preserved"),"Configuration fixture publication failed");
 require(app.run({"doctor","--offline"},output)==1&&read(config)=="not-json; preserved","Doctor replaced invalid configuration");
 fs::remove(config);require(publish(config,"{}\n"),"Valid configuration fixture publication failed");
 require(app.run({"doctor"},output)==0&&read(config)=="{}\n","Doctor altered existing configuration contents");
 const auto pending=WindowsRunSession::pendingClaimPath("cuda-7");require(publish(pending,"uncertain claim preserved"),"Pending fixture publication failed");
 std::ostringstream all;require(app.run({"status","--all"},all)==2&&all.str().find("Slot: cuda-7")!=std::string::npos&&read(pending)=="uncertain claim preserved","Status omitted or changed pending-only slot");
 execute({"status","--slot","cuda-7"},2,"Pending claim: preserved");fs::remove(pending);
 const auto state=Store::path("primary");require(publish(state,"unreadable state bytes"),"State fixture publication failed");
 require(app.run({"status"},output)==2&&app.run({"doctor","--offline"},output)==2&&read(state)=="unreadable state bytes","Diagnostics cleaned or hid unreadable execution state");
 fs::remove_all(root);std::cout<<"WindowsCliApplicationTests passed: real executable, Unicode argv/paths, fresh profile, local controls, no automatic search and preserved invalid data\n";return 0;
}catch(const std::exception& error){Control::release();std::cerr<<error.what()<<'\n';return 1;}}
