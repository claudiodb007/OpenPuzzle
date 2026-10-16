#include <windows.h>
#include "openpuzzle/hardware/GpuControl.hpp"
#include <fstream>
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <iostream>
#include <string>
#include <vector>
int wmain(int argc,wchar_t** argv){
 const std::wstring command=argc>1?argv[1]:L"";
 if(command==L"gpu-control"){
  using namespace openpuzzle;
  struct Fake:GpuControlBackend {
   std::filesystem::path path=localPaths::dataDirectory()/"gpu-control-fixture.txt";
   std::vector<GpuControlDevice> inspect()override{
    std::ifstream file(path);if(file){const std::string text((std::istreambuf_iterator<char>(file)),{});return parseGpuControls(text);}
    GpuControlDevice d;d.vendor="nvidia";d.pci="00000000:08:00.0";d.uuid="GPU-fixture";d.name="NVIDIA fixture";d.power=d.fan=d.automatic=true;d.powerNow=d.powerDefault=300;d.powerMin=100;d.powerMax=333;d.powerStep=.001;d.fanNow=40;d.fanMode="auto";d.temperature=67;
    auto a=d;a.vendor="amd";a.pci="00000000:09:00.0";a.uuid.clear();a.name="AMD fixture";a.powerUnit="%";a.powerNow=-10;a.powerDefault=0;a.powerMin=-50;a.powerMax=20;a.powerStep=1;return {d,a};
   }
   void apply(const GpuControlDevice& selected,const GpuControlRequest& r)override{
    auto all=inspect();for(auto& d:all)if(d.pci==selected.pci){if(r.action==GpuControlAction::Power)d.powerNow=r.value;if(r.action==GpuControlAction::PowerDefault)d.powerNow=d.powerDefault;
     if(r.action==GpuControlAction::FanManual){d.fanMode="manual";d.fanNow=r.value;}if(r.action==GpuControlAction::FanAuto)d.fanMode="auto";}
    Sleep(300);WorkspaceSecurity::prepare(path.parent_path());if(!writePrivateFileAtomically(path,formatGpuControls(all)))throw std::runtime_error("Fixture state write failed");
   }
  } fake;
  std::vector<std::string> args;for(int i=2;i<argc;++i)args.push_back(pathUtf8(std::filesystem::path(argv[i])));
  try{return gpuControlCommand(args,std::cout,[&]{return fake.inspect();},[&](const auto& r){applyGpuControlChecked(fake,r);});}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 }
 if(command==L"echo"){for(int i=2;i<argc;++i)std::cout<<openpuzzle::pathUtf8(std::filesystem::path(argv[i]))<<'\n';return 0;}
 if(command==L"slow"||command==L"fixture-run"){
  const auto marker=openpuzzle::localPaths::dataDirectory()/"ui-live-speed-fixture";
  if(command==L"fixture-run"){openpuzzle::WorkspaceSecurity::prepare(marker.parent_path());if(!openpuzzle::writePrivateFileAtomically(marker,"running"))return 8;}
  std::cout<<"runtime-started\n"<<std::flush;
  if(command==L"slow")Sleep(5000);else {const auto until=GetTickCount64()+5000;while(!std::filesystem::exists(marker.parent_path()/"ui-speed-observed")&&GetTickCount64()<until)Sleep(10);Sleep(600);}
  if(command==L"fixture-run"){std::filesystem::remove(marker);std::filesystem::remove(marker.parent_path()/"ui-speed-observed");}std::cout<<"runtime-finished\n";return 0;
 }
 if(command==L"flood"){std::cout<<std::string(300000,'x')<<"\nTAIL\n";return 0;}
 if(command==L"fail"){std::cout<<"fixture-error\n";return 9;}
 if(command==L"run"){
  bool dry=false;for(int i=2;i<argc;++i)if(std::wstring(argv[i])==L"--dry-run")dry=true;
  if(!dry)return 8;std::cout<<"benchmark-profile-preflight-fixture\n";return 0;
 }
 if(command==L"benchmark"){
  const std::vector<std::wstring> expected{L"benchmark",L"--backend",L"cpu",L"--device",L"0",L"--threads",L"1",L"--seconds",L"30",L"--real"};
  if(argc!=static_cast<int>(expected.size()+1))return 8;for(int i=1;i<argc;++i)if(argv[i]!=expected[i-1])return 8;
  const auto report=openpuzzle::localPaths::dataDirectory()/"engine-diagnostics"/"ui-benchmark-fixture";openpuzzle::WorkspaceSecurity::prepare(report);
  const auto json="{\"version\":1,\"backend\":\"cpu\",\"engine_sha256\":\""+std::string(64,'a')+"\",\"device\":0,\"device_identity\":\"CPU fixture\",\"selftest_passed\":true,\"threads\":1,\"blocks\":256,\"points\":256,\"speed_mkeys\":2.0,\"samples\":3,\"duration_seconds\":30}\n";
  Sleep(300);if(!openpuzzle::writePrivateFileAtomically(report/"result.json",json))return 8;
  std::cout<<"Report: "<<openpuzzle::pathUtf8(report)<<"\nbenchmark-fixture-finished\n";return 0;
 }
 if(command==L"status"){
  if(std::filesystem::exists(openpuzzle::localPaths::dataDirectory()/"ui-live-speed-fixture")){if(!openpuzzle::writePrivateFileAtomically(openpuzzle::localPaths::dataDirectory()/"ui-speed-observed","observed"))return 8;std::cout<<"Slot: primary\nRuntime: running\nAssignment: fixture-search\nPending claim: none\nSpeed MKey/s: 1379.58\nKeys checked: 1720000000000\n";}
  else std::cout<<"Slot: primary\nRuntime: idle\nAssignment: none\nPending claim: none\nSpeed MKey/s: unavailable\nKeys checked: unavailable\n";return 0;
 }
 if(command==L"telemetry"){std::cout<<"GPU telemetry: AVAILABLE\nDevice: synthetic\nTemperature C: 31\nPower W: 10\nPower limit W: 300\n";return 0;}
 if(command==L"thermal"){std::cout<<"Thermal mode: enabled\nWarning C: 75\nCritical C: 85\nStop on critical: yes\n";return 0;}
 if(command==L"stop"||command==L"safestop"){std::cout<<"Cooperative request recorded; completion is not yet confirmed\n";return 0;}
 return 8;
}
