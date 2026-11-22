#include "openpuzzle/core/WindowsCliApplication.hpp"
#include "openpuzzle/runtime/WindowsProfileGpuBinding.hpp"
#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace openpuzzle;namespace fs=std::filesystem;
namespace {
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
const std::string uuidA="12345678-1234-5678-9abc-123456789abc",uuidB="22345678-1234-5678-9abc-123456789abc";
}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Engine fixture required");const auto root=fs::temp_directory_path()/pathFromUtf8("op-bind-cli-"+std::to_string(GetCurrentProcessId())+'-'+std::to_string(GetTickCount64()));
 fs::create_directory(root);require(SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str())&&SetEnvironmentVariableA("OPENPUZZLE_EXECUTION_SLOT","primary")&&SetEnvironmentVariableA("OPENPUZZLE_BINDING_FIXTURE_MODE",nullptr),"Fixture environment failed");
 const auto engines=root/"engines";fs::create_directory(engines);for(const auto* name:{"cuBitCrack.exe","clBitCrack.exe"})fs::copy_file(fs::absolute(argv[1]),engines/name);
 bool switched=false;int discovered=0;
 auto discovery=[&]{++discovered;WindowsGpuInventory i;i.nativeCpuAvailable=true;WindowsGpuBackend b;b.state=GpuDiscoveryState::Available;
  for(int n=2;n<=3;++n){WindowsGpuDevice d;d.name="fixture GPU";d.vendor="NVIDIA";d.memoryMiB=10240;d.nativeDeviceIndex=n;d.platformIndex=0;d.available=true;
   d.pciBusId=n==2?"0000:08:00.0":"0000:09:00.0";d.uuid=(n==2)!=switched?uuidA:uuidB;b.devices.push_back(d);}
  for(auto& d:b.devices)d.compilerAvailable=true;
  i.cuda=b;i.opencl=b;return i;};
 auto telemetry=[]{WindowsTelemetryReport r{true,"fixture",{}};for(int n=1;n>=0;--n){client::ClientGpuTelemetry v;v.vendor="NVIDIA";v.device=n;v.deviceId="nvidia-"+std::to_string(n);v.pciBusId=n?"0000:09:00.0":"0000:08:00.0";v.temperatureC=n?95:60;r.readings.push_back({"fixture GPU",v,n?uuidB:uuidA});}return r;};
 WindowsCliApplication app({},discovery,engines,telemetry);std::ostringstream out;
 for(const auto& bad:std::vector<std::vector<std::string>>{{"bind-profile"},{"bind-profile","--profile","relative","--output","relative"},{"bind-profile","--server","http://127.0.0.1:1"},{"bind-profile","--slot","cuda-0"}})
  require(app.run(bad,out)==1&&discovered==0,"Invalid options contacted a driver");
 require(!fs::exists(localPaths::root()),"Invalid options created profile storage");
 for(const auto& backend:{"cuda","opencl"}){
  std::ostringstream listed;
  require(app.run({"engine-devices","--backend",backend},listed)==0&&listed.str().find("\"native_index\":2")!=std::string::npos,
   "UI engine device list did not preserve verified native mapping");
 }
 for(const auto* backend:{"cuda","opencl"}){
  const auto executable=engines/(std::string(backend)=="cuda"?"cuBitCrack.exe":"clBitCrack.exe");
  const auto identity=WindowsEngineDiagnostics::inspectDevice(executable,backend,0,32);
  const auto source=root/(std::string(backend)+".json"),dest=root/(std::string(backend)+"-bound.json");
  const std::string content="{\"version\":1,\"backend\":\""+std::string(backend)+"\",\"engine_sha256\":\""+WindowsEngineDiagnostics::executableSha256(executable)+"\",\"device\":0,\"device_identity\":\""+identity+"\",\"selftest_passed\":true,\"threads\":32,\"blocks\":1,\"points\":1,\"samples\":3,\"duration_seconds\":10,\"speed_mkeys\":123.456}";
  require(writePrivateFileAtomically(source,content),"Fixture profile publication failed");
  require(app.run({"bind-profile","--profile",pathUtf8(source),"--output",pathUtf8(dest)},out)==0,"Binding failed");
  const auto p=WindowsRunProfile::parse(read(dest));require(p.gpuUuid==uuidA&&p.pciBusId=="00000000:08:00.0"&&p.speed==123.456&&read(source)==content,"Binding changed measured data or selected the hot same-name device");
  const auto saved=read(dest);require(app.run({"bind-profile","--profile",pathUtf8(source),"--output",pathUtf8(dest)},out)==1&&read(dest)==saved,"Existing output was overwritten");
  switched=true;const auto refused=root/(std::string(backend)+"-refused.json");require(app.run({"bind-profile","--profile",pathUtf8(source),"--output",pathUtf8(refused)},out)==1&&!fs::exists(refused),"Inconsistent UUID/PCI mapping published a profile");switched=false;
  require(app.run({"bind-profile","--profile",pathUtf8(dest),"--output",pathUtf8(refused)},out)==1&&!fs::exists(refused),"Already-bound profile was rebound");
 }
 // The actual CLI handoff must publish an AMD/OpenCL profile without NVML,
 // including on systems where the optional sensor provider throws.
 auto amdDiscovery=[&]{auto i=discovery();for(auto& d:i.opencl.devices)d.vendor="Advanced Micro Devices, Inc.";return i;};
 auto noSensors=[]()->WindowsTelemetryReport{throw std::runtime_error("NVML is not installed");};
 WindowsCliApplication amdApp({},amdDiscovery,engines,noSensors);
 const auto amdDest=root/"amd-bound.json";
 require(amdApp.run({"bind-profile","--profile",pathUtf8(root/"opencl.json"),"--output",pathUtf8(amdDest)},out)==0,
  "AMD profile was blocked by missing NVIDIA sensors");
 const auto amdProfile=WindowsRunProfile::parse(read(amdDest));
 require(amdProfile.gpuUuid==uuidA&&amdProfile.speed==123.456,"AMD profile lost physical identity or measured speed");
 bool thermalRefused=false;
 try{inspectWindowsProfileGpuBinding(amdProfile,engines/"clBitCrack.exe",amdDiscovery,noSensors,true);}catch(...){thermalRefused=true;}
 require(thermalRefused,"Critical-temperature policy silently accepted missing AMD sensors");
 auto legacyDiscovery=[&]{auto i=amdDiscovery();for(auto& d:i.opencl.devices){d.uuid.clear();d.pciBusId.clear();d.windowsDeviceInstanceId=d.nativeDeviceIndex==2?"PCI\\VEN_1002&DEV_731F\\RIG-A":"PCI\\VEN_1002&DEV_731F\\RIG-B";}return i;};
 WindowsCliApplication legacyApp({},legacyDiscovery,engines,noSensors);const auto legacyDest=root/"legacy-amd-bound.json";
 require(legacyApp.run({"bind-profile","--profile",pathUtf8(root/"opencl.json"),"--output",pathUtf8(legacyDest)},out)==0,"Legacy AMD Windows identity was not published by the actual CLI");
 const auto legacy=WindowsRunProfile::parse(read(legacyDest));
 require(legacy.hasPhysicalBinding()&&legacy.gpuUuid.empty()&&legacy.pciBusId.empty()&&legacy.windowsDeviceInstanceId=="PCI\\VEN_1002&DEV_731F\\RIG-A","Legacy AMD identity was lost or a PCI segment was fabricated");
 // Reproduce the actual RX 5700 XT report: chip name, WGP count, usable
 // memory and persistent Windows identity, with no CUDA GPU or NVML dependency.
 require(SetEnvironmentVariableA("OPENPUZZLE_BINDING_FIXTURE_MODE","amd-5700xt"),"AMD fixture mode");
 const auto rxDiscovery=[](){WindowsGpuInventory i;i.nativeCpuAvailable=true;
  i.cuda={GpuDiscoveryState::Empty,"CUDA driver reports no device",{}};
  WindowsGpuDevice d;d.name="gfx1010:xnack-";d.displayName="AMD Radeon RX 5700 XT";d.vendor="Advanced Micro Devices, Inc.";
  d.driverVersion="3652.0 (PAL,LC)";d.memoryMiB=8176;d.computeUnits=20;d.nativeDeviceIndex=0;d.platformIndex=0;d.available=true;d.compilerAvailable=true;
  d.windowsDeviceInstanceId="PCI\\VEN_1002&DEV_731F&SUBSYS_E4101DA2&REV_C1\\6&EAF386A&0&00000019";
  i.opencl={GpuDiscoveryState::Available,"fixture",{d}};return i;};
 WindowsCliApplication rxApp({},rxDiscovery,engines,noSensors);std::ostringstream rxDevices;
 require(rxApp.run({"engine-devices","--backend","opencl"},rxDevices)==0&&rxDevices.str().find("AMD Radeon RX 5700 XT")!=std::string::npos&&rxDevices.str().find("gfx1010:xnack-")!=std::string::npos,"RX 5700 XT engine discovery failed");
 WindowsCliApplication missingCuda({},rxDiscovery,root/"engines-do-not-exist",noSensors);std::ostringstream noCuda;
 require(missingCuda.run({"engine-devices","--backend","cuda"},noCuda)==1&&noCuda.str().find("CUDA driver reports no device")!=std::string::npos,"Missing CUDA driver still reached the CUDA executable");
 require(SetEnvironmentVariableA("OPENPUZZLE_BINDING_FIXTURE_MODE",nullptr),"AMD fixture cleanup");
 require(!fs::exists(localPaths::configDirectory()/"client.id"),"Profile binding created a client identity");
 std::cout<<"WindowsProfileBindingCliTests passed: real engine enumeration, duplicate names, preserved measured copies and refusal paths\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
