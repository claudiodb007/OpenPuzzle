#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/ui/WindowsUiModel.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace openpuzzle;namespace fs=std::filesystem;
void check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
int wmain(int argc,wchar_t** argv){try{
 check(argc==2,"Engine binding fixture required");
 const auto root=fs::temp_directory_path()/pathFromUtf8("OpenPuzzle-binding ç 中文 & literal-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));fs::create_directories(root);
 const auto profile=root/"profile";check(SetEnvironmentVariableW(L"LOCALAPPDATA",profile.c_str())&&SetEnvironmentVariableW(L"OPENPUZZLE_EXECUTION_SLOT",nullptr),"Private binding environment failed");
 const auto engines=root/pathFromUtf8("engines é 中文 & literal");fs::create_directory(engines);
 for(const auto name:{"keyhunt.exe","clBitCrack.exe","cuBitCrack.exe"})fs::copy_file(fs::absolute(argv[1]),engines/name);
 WindowsCliApplication app({},[]{WindowsGpuInventory i;i.nativeCpuAvailable=true;i.logicalProcessors=8;return i;},engines);std::ostringstream out;
 for(const auto& args:std::vector<std::vector<std::string>>{
  {"selftest"},{"selftest","--backend","other"},{"selftest","--backend","cpu","--device","1"},
  {"selftest","--backend","cuda","--device","0x"},{"selftest","--backend","cpu","--backend","cpu"},
  {"benchmark","--backend","cpu","--threads","0"},{"benchmark","--backend","cpu","--blocks","1"},
  {"benchmark","--backend","cuda","--threads","33"},{"benchmark","--backend","cuda","--seconds","1"},
  {"selftest","--backend","cpu","--slot","primary"},{"selftest","--backend","cpu","--real"}})
  check(app.run(args,out)==1,"Malformed local engine arguments accepted");
 check(!fs::exists(localPaths::root()),"Invalid engine commands created a profile");
 check(app.run({"benchmark","--backend","cpu"},out)==0&&!fs::exists(localPaths::root()),"Benchmark preview started a motor or wrote data");
 for(const auto& backend:{"cpu","cuda","opencl"}){
  ui::WindowsUiSelection selection;WindowsRunProfile p;p.backend=backend;p.device=0;p.threads=p.backend=="cpu"?1:256;p.blocks=256;p.points=1024;selection.profile=p;
  const auto preview=ui::windowsUiCommand(ui::UiAction::BenchmarkPreview,selection);
  check(app.run(preview,out)==0&&!fs::exists(localPaths::root()),"UI benchmark preview launched an engine or wrote state");
  const auto real=WindowsEngineCheckOptions::parse(ui::windowsUiCommand(ui::UiAction::Benchmark,selection));
  check(real.real&&real.backend==backend&&real.device==0&&real.threads==p.threads&&real.seconds==30,"UI benchmark parameters did not reach the native diagnostic parser");
 }
 const auto fingerprint=WindowsEngineDiagnostics::executableSha256(engines/"keyhunt.exe");check(fingerprint.size()==64&&fingerprint==WindowsEngineDiagnostics::executableSha256(engines/"clBitCrack.exe"),"Executable hash is inconsistent");
 for(const auto backend:{"cpu","cuda","opencl"})check(app.run({"selftest","--backend",backend},out)==0,"Valid native engine binding failed");
 const auto data=localPaths::dataDirectory()/"engine-diagnostics";std::size_t good=0;
 for(const auto& entry:fs::directory_iterator(data))if(fs::exists(entry.path()/"result.json"))++good;
 check(good==3&&!fs::exists(localPaths::configDirectory()/"client.id")&&!fs::exists(localPaths::configDirectory()/"config.json"),"Self-test created identity/configuration or lost its report");
 auto fail=[&](const wchar_t* mode,const char* backend){check(SetEnvironmentVariableW(L"OPENPUZZLE_BINDING_FIXTURE_MODE",mode),"Fixture mode failed");check(app.run({"selftest","--backend",backend},out)==1,"Bad native engine output was accepted");check(SetEnvironmentVariableW(L"OPENPUZZLE_BINDING_FIXTURE_MODE",nullptr),"Fixture reset failed");};
 fail(L"wrong-protocol","cpu");fail(L"bad-json","cuda");fail(L"wrong-wif","opencl");fail(L"overshoot","cpu");fail(L"search-failure","cuda");
 check(app.run({"selftest","--backend","cuda","--device","1"},out)==1,"Unknown engine index was accepted");
 std::size_t stillGood=0;for(const auto& entry:fs::directory_iterator(data))if(fs::exists(entry.path()/"result.json"))++stillGood;
 check(stillGood==3,"A failed binding published success or overwrote prior results");
 check(ClientRuntimeControl::acquire(),"Runtime fixture acquisition failed");
 check(app.run({"selftest","--backend","cpu"},out)==1,"Engine diagnostic competed with an existing runtime");check(ClientRuntimeControl::release(),"Runtime fixture release failed");
 check(SetEnvironmentVariableW(L"OPENPUZZLE_BINDING_FIXTURE_MODE",L"early-exit"),"Early fixture setup failed");
 check(app.run({"benchmark","--backend","cpu","--real","--seconds","10","--threads","1"},out)==1,"One-second CPU benchmark accepted as sustained");
 check(SetEnvironmentVariableW(L"OPENPUZZLE_BINDING_FIXTURE_MODE",nullptr),"Benchmark fixture reset failed");
 check(app.run({"benchmark","--backend","cpu","--real","--seconds","10","--threads","1"},out)==0,"Sustained benchmark did not publish verified speed");
 check(out.str().find("Median speed: 1.000 MKey/s")!=std::string::npos,"CPU benchmark speed was divided by two");
 fs::remove_all(root);std::cout<<"WindowsEngineBindingTests passed: native processes, Unicode/literal paths, CPU/CUDA/OpenCL mapping, bounded known keys, overshoot refusal, preserved failures and sustained CPU samples\n";return 0;
}catch(const std::exception& e){ClientRuntimeControl::release();std::cerr<<e.what()<<'\n';return 1;}}
