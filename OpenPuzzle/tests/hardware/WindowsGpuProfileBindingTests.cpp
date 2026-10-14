#include "openpuzzle/engines/WindowsGpuProfileBinding.hpp"
#include <iostream>
using namespace openpuzzle;
namespace {
void require(bool ok){if(!ok)throw std::runtime_error("Physical profile binding contract failed");}
template<class F>void refused(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed);}
const std::string uuidA="12345678-1234-5678-9abc-123456789abc",uuidB="22345678-1234-5678-9abc-123456789abc";
std::string json(){return "{\"version\":1,\"backend\":\"cuda\",\"engine_sha256\":\""+std::string(64,'a')+"\",\"device\":3,\"device_identity\":\"same GPU; platform=0; native=3; memory=10737418240\",\"threads\":256,\"blocks\":256,\"points\":1024,\"selftest_passed\":true,\"samples\":8,\"duration_seconds\":30,\"speed_mkeys\":123.4567890123}\n";}
WindowsTelemetryReading sensor(const std::string& uuid,const std::string& pci,double temp){client::ClientGpuTelemetry v;v.vendor="NVIDIA";v.device=0;v.deviceId="nvidia-0";v.pciBusId=pci;v.temperatureC=temp;return {"same GPU",v,uuid};}
}
int main(){try{
 require(canonicalGpuUuid("GPU-12345678-1234-5678-9ABC-123456789ABC")==uuidA);
 for(const auto& s:{std::string{},std::string("MIG-")+uuidA,std::string("00000000-0000-0000-0000-000000000000"),uuidA+"junk"})require(canonicalGpuUuid(s).empty());
 const auto p=WindowsRunProfile::parse(json());require(p.gpuUuid.empty()&&p.pciBusId.empty());
 auto engine=WindowsEngineGpuIdentity::parse(p.deviceIdentity);require(engine.platform==0&&engine.native==3&&engine.memoryBytes==10737418240ULL);
 for(const auto& s:{"name; platform=0; native=x; memory=10737418240","name; platform=0; native=3; memory=0","name; platform=64; native=3; memory=10737418240"})refused([&]{WindowsEngineGpuIdentity::parse(s);});
 std::vector<WindowsProfileGpuDevice> devices={{0,2,"same GPU","NVIDIA","0000:09:00.0",uuidB,10240,true},{0,3,"same GPU","NVIDIA","0000:08:00.0",uuidA,10240,true}};
 WindowsTelemetryReport r{true,"fixture",{sensor(uuidB,"0000:09:00.0",95),sensor(uuidA,"0000:08:00.0",60)}};
 const auto binding=resolveWindowsGpuProfileBinding(p,devices,r);require(binding.uuid==uuidA&&binding.pciBusId=="00000000:08:00.0");
 const auto boundJson=appendWindowsGpuProfileBinding(json(),binding);const auto bound=WindowsRunProfile::parse(boundJson);
 require(bound.speed==p.speed&&bound.device==p.device&&bound.threads==p.threads&&bound.points==p.points&&bound.sha256==p.sha256&&bound.deviceIdentity==p.deviceIdentity&&bound.gpuUuid==uuidA);
 require(boundJson.starts_with(json().substr(0,json().find_last_of('}'))));
 require(resolveWindowsGpuProfileBinding(bound,devices,r).uuid==uuidA);
 refused([&]{appendWindowsGpuProfileBinding(boundJson,binding);});
 auto swapped=devices;std::swap(swapped[0].uuid,swapped[1].uuid);std::swap(swapped[0].pciBusId,swapped[1].pciBusId);
 refused([&]{resolveWindowsGpuProfileBinding(bound,swapped,r);});
 auto bad=devices;bad.push_back(devices[1]);refused([&]{resolveWindowsGpuProfileBinding(p,bad,r);});
 bad=devices;bad[0].uuid=uuidA;refused([&]{resolveWindowsGpuProfileBinding(p,bad,r);});
 bad=devices;bad[1].memoryMiB=8192;refused([&]{resolveWindowsGpuProfileBinding(p,bad,r);});
 bad=devices;bad[1].available=false;refused([&]{resolveWindowsGpuProfileBinding(p,bad,r);});
 bad=devices;bad[1].uuid.clear();bad[1].pciBusId.clear();refused([&]{resolveWindowsGpuProfileBinding(p,bad,r);});
 auto missing=r;missing.complete=false;refused([&]{resolveWindowsGpuProfileBinding(p,devices,missing);});
 missing=r;missing.readings.erase(missing.readings.begin()+1);refused([&]{resolveWindowsGpuProfileBinding(p,devices,missing);});
 missing=r;missing.readings.push_back(missing.readings[1]);refused([&]{resolveWindowsGpuProfileBinding(p,devices,missing);});
 auto pciOnly=devices;pciOnly[1].uuid.clear();require(resolveWindowsGpuProfileBinding(p,pciOnly,r).pciBusId==binding.pciBusId);
 require(resolveWindowsGpuProfileBinding(bound,pciOnly,r).uuid==uuidA);
 auto uuidOnly=devices;uuidOnly[1].pciBusId.clear();require(resolveWindowsGpuProfileBinding(bound,uuidOnly,r).pciBusId==binding.pciBusId);
 WindowsRuntimeThermalSettings settings;settings.policy={true,true,75,85};settings.deviceName=binding.name;settings.devicePciBusId=binding.pciBusId;settings.deviceUuid=binding.uuid;settings.reader=[&]{return r;};
 auto now=std::chrono::steady_clock::now();WindowsThermalMonitor monitor(settings);monitor.sample(now);require(!monitor.mustStop()&&monitor.status()==WindowsThermalStatus::Normal);
 r.readings[1].values.temperatureC=90;monitor.sample(now+std::chrono::seconds(5));require(monitor.mustStop()&&monitor.status()==WindowsThermalStatus::Critical);
 r.readings[1].values.temperatureC=60;r.readings[1].uuid=uuidB;monitor.sample(now+std::chrono::seconds(10));require(monitor.mustStop()&&monitor.status()==WindowsThermalStatus::Unavailable);
 settings.deviceUuid.clear();r.readings[1].uuid=uuidA;WindowsThermalMonitor pciMonitor(settings);pciMonitor.sample(now);require(!pciMonitor.mustStop());
 r.readings[0].values.pciBusId="0000:08:00.0";WindowsThermalMonitor duplicate(settings);duplicate.sample(now);require(duplicate.mustStop());
 std::cout<<"WindowsGpuProfileBindingTests passed: duplicate names, reordered readings, physical swaps, unavailable sensors and unchanged measurements\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
