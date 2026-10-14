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
  i.cuda=b;i.opencl=b;return i;};
 auto telemetry=[]{WindowsTelemetryReport r{true,"fixture",{}};for(int n=1;n>=0;--n){client::ClientGpuTelemetry v;v.vendor="NVIDIA";v.device=n;v.deviceId="nvidia-"+std::to_string(n);v.pciBusId=n?"0000:09:00.0":"0000:08:00.0";v.temperatureC=n?95:60;r.readings.push_back({"fixture GPU",v,n?uuidB:uuidA});}return r;};
 WindowsCliApplication app({},discovery,engines,telemetry);std::ostringstream out;
 for(const auto& bad:std::vector<std::vector<std::string>>{{"bind-profile"},{"bind-profile","--profile","relative","--output","relative"},{"bind-profile","--server","http://127.0.0.1:1"},{"bind-profile","--slot","cuda-0"}})
  require(app.run(bad,out)==1&&discovered==0,"Invalid options contacted a driver");
 require(!fs::exists(localPaths::root()),"Invalid options created profile storage");
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
 require(!fs::exists(localPaths::configDirectory()/"client.id"),"Profile binding created a client identity");
 std::cout<<"WindowsProfileBindingCliTests passed: real engine enumeration, duplicate names, preserved measured copies and refusal paths\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
