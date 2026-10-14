#include "openpuzzle/hardware/WindowsGpuTelemetry.hpp"
#include "openpuzzle/hardware/WindowsGpuDiscovery.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <set>
#include <sstream>
#include <limits>
namespace openpuzzle {
namespace {
static_assert(sizeof(WindowsNvmlPciInfo)==68 && sizeof(void*)==8);
template<std::size_t N>std::string text(const char(&s)[N]){
 const auto end=std::find(s,s+N,0);if(end==s+N||end==s)throw std::runtime_error("NVML string is empty or unterminated");
 std::string v(s,end);if(std::any_of(v.begin(),v.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid NVML text");return v;
}
void check(int code,const char* what){if(code)throw std::runtime_error(std::string(what)+" failed (NVML code "+std::to_string(code)+")");}
std::string pci(unsigned domain,unsigned bus,unsigned device){
 if(bus>255||device>31)throw std::runtime_error("Invalid PCI identity");
 std::ostringstream s;s<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(8)<<domain<<':'<<std::setw(2)<<bus<<':'<<std::setw(2)<<device<<".0";return s.str();
}
class Module {
 HMODULE h_=nullptr;
public:
 explicit Module(const std::wstring& path):h_(LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32)){}
 ~Module(){if(h_)FreeLibrary(h_);}
 bool valid()const{return h_!=nullptr;}
 template<class T>T function(const char* n)const{return reinterpret_cast<T>(GetProcAddress(h_,n));}
};
}
std::wstring WindowsGpuTelemetry::systemNvmlPath(){
 std::wstring s(32768,L'\0');const auto n=GetSystemDirectoryW(s.data(),static_cast<unsigned>(s.size()));
 if(!n||n>=s.size())throw std::runtime_error("Windows system directory unavailable");s.resize(n);return s+L"\\nvml.dll";
}
WindowsTelemetryReport WindowsGpuTelemetry::query(const WindowsNvmlApi& a){
 WindowsTelemetryReport out;
 try{
  if(!a.init||!a.shutdown||!a.count||!a.handle||!a.name||!a.pci)throw std::runtime_error("NVML identity exports unavailable");
  check(a.init(),"NVML initialization");struct Guard{const WindowsNvmlApi& a;~Guard(){(void)a.shutdown();}}guard{a};
  unsigned count=0;check(a.count(&count),"NVML count");if(count>256)throw std::runtime_error("NVML count exceeds limit");
  std::set<void*> handles;std::set<std::string> buses,uuids;
  for(unsigned i=0;i<count;++i){
   void* h=nullptr;check(a.handle(i,&h),"NVML handle");if(!h||!handles.insert(h).second)throw std::runtime_error("Duplicate NVML handle");
   char name[96];std::fill(std::begin(name),std::end(name),1);check(a.name(h,name,sizeof(name)),"NVML name");
   WindowsNvmlPciInfo info{};check(a.pci(h,&info),"NVML PCI identity");const auto bus=canonicalGpuPciBusId(text(info.busId));
   const auto numeric=pci(info.domain,info.bus,info.device);
   if(bus.empty()||bus.substr(0,bus.size()-1)!=numeric.substr(0,numeric.size()-1))throw std::runtime_error("Inconsistent NVML PCI identity");
   if(!buses.insert(bus).second)throw std::runtime_error("Duplicate PCI identity");
   WindowsTelemetryReading r;r.name=text(name);r.values.vendor="NVIDIA";r.values.device=static_cast<int>(i);r.values.deviceId="nvidia-"+std::to_string(i);r.values.pciBusId=bus;r.values.temperatureLabel="GPU";
   if(a.uuid){char value[96];std::fill(std::begin(value),std::end(value),1);
    if(a.uuid(h,value,sizeof(value))==0){r.uuid=canonicalGpuUuid(text(value));
     if(r.uuid.empty()||!uuids.insert(r.uuid).second)throw std::runtime_error("Invalid or duplicate NVML UUID");}}
   unsigned value=0;
   if(a.temperature&&a.temperature(h,0,&value)==0&&value<=200)r.values.temperatureC=static_cast<double>(value);
   if(a.power&&a.power(h,&value)==0&&value<=10000000)r.values.powerDrawW=value/1000.0;
   if(a.limit&&a.limit(h,&value)==0&&value>0&&value<=10000000)r.values.powerLimitW=value/1000.0;
   out.readings.push_back(std::move(r));
  }
  unsigned finalCount=0;check(a.count(&finalCount),"NVML final count");if(count!=finalCount)throw std::runtime_error("NVML inventory changed");
  out.complete=true;out.detail=count?"Read-only NVIDIA NVML telemetry":"NVML reports no NVIDIA GPU";
 }catch(const std::exception& e){out.complete=false;out.readings.clear();out.detail=e.what();}return out;
}
WindowsTelemetryReport WindowsGpuTelemetry::inspect(){
 try{
  // DCH driver path. No PATH/current-directory search and no settings writes.
  Module module(systemNvmlPath());if(!module.valid())return {false,"System NVML unavailable (Windows code "+std::to_string(GetLastError())+")",{}};
  WindowsNvmlApi a;
  a.init=module.function<decltype(a.init)>("nvmlInit_v2");a.shutdown=module.function<decltype(a.shutdown)>("nvmlShutdown");
  a.count=module.function<decltype(a.count)>("nvmlDeviceGetCount_v2");a.handle=module.function<decltype(a.handle)>("nvmlDeviceGetHandleByIndex_v2");
  a.name=module.function<decltype(a.name)>("nvmlDeviceGetName");a.pci=module.function<decltype(a.pci)>("nvmlDeviceGetPciInfo_v3");
  a.temperature=module.function<decltype(a.temperature)>("nvmlDeviceGetTemperature");a.power=module.function<decltype(a.power)>("nvmlDeviceGetPowerUsage");a.limit=module.function<decltype(a.limit)>("nvmlDeviceGetPowerManagementLimit");
  a.uuid=module.function<decltype(a.uuid)>("nvmlDeviceGetUUID");
  auto report=query(a);
  // CUDA ordinals are resolved by physical PCI identity, not NVML enumeration.
  const auto inventory=WindowsGpuDiscovery::inspect();
  if(inventory.cuda.state==GpuDiscoveryState::Available){
   std::vector<std::pair<std::string,int>> devices;
   for(const auto& d:inventory.cuda.devices)devices.emplace_back(d.pciBusId,d.nativeDeviceIndex);
   report.bindCudaOrdinals(devices);
  }return report;
 }catch(const std::exception& e){return {false,e.what(),{}};}
}
std::string formatWindowsTelemetry(const WindowsTelemetryReport& r,const GpuThermalPolicyConfiguration& p){
 std::ostringstream s;s<<"GPU telemetry: "<<(r.complete?"AVAILABLE":"UNAVAILABLE")<<"\nTelemetry detail: "<<r.detail<<'\n';
 for(const auto& d:r.readings){const auto& v=d.values;s<<"Device: "<<d.name<<" ("<<v.deviceId<<", "<<v.pciBusId<<")\n";
  s<<"GPU UUID: "<<(d.uuid.empty()?"unavailable":d.uuid)<<'\n';
  s<<"Temperature C: ";if(v.temperatureC)s<<*v.temperatureC;else s<<"unavailable";s<<"\nPower W: ";if(v.powerDrawW)s<<*v.powerDrawW;else s<<"unavailable";s<<"\nPower limit W: ";if(v.powerLimitW)s<<*v.powerLimitW;else s<<"unavailable";
  s<<"\nThermal state: "<<GpuThermalPolicy::stateName(GpuThermalPolicy::evaluate(p,v.temperatureC))<<'\n';
 }return s.str();
}
}
