#pragma once
#include "openpuzzle/client/ClientHeartbeat.hpp"
#include "openpuzzle/hardware/GpuThermalPolicy.hpp"
#include "openpuzzle/hardware/WindowsAdapterIdentity.hpp"
#include <set>
#include <optional>
#include <cmath>
#include <chrono>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <functional>
#include <stdexcept>
#include <vector>
#include <array>
#include <algorithm>
#include <cctype>
namespace openpuzzle {
inline std::string canonicalGpuUuid(std::string value) {
 if(value.starts_with("GPU-"))value.erase(0,4);
 if(value.size()!=36)return {};
 bool nonzero=false;
 for(std::size_t i=0;i<value.size();++i){
  if(i==8||i==13||i==18||i==23){if(value[i]!='-')return {};continue;}
  const auto c=static_cast<unsigned char>(value[i]);
  if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return {};
  value[i]=static_cast<char>(std::tolower(c));nonzero=nonzero||value[i]!='0';
 }
 return nonzero?value:std::string{};
}
inline std::string gpuUuidFromBytes(const std::array<unsigned char,16>& bytes) {
 std::ostringstream out;out<<std::hex<<std::setfill('0');
 for(std::size_t i=0;i<bytes.size();++i){if(i==4||i==6||i==8||i==10)out<<'-';out<<std::setw(2)<<static_cast<unsigned>(bytes[i]);}
 return canonicalGpuUuid(out.str());
}
inline std::string canonicalGpuPciBusId(const std::string& value){
 const auto first=value.find(':'),second=value.find(':',first==std::string::npos?0:first+1),dot=value.find('.',second==std::string::npos?0:second+1);
 if(first==std::string::npos||second==std::string::npos||dot==std::string::npos||first<1||first>8||second-first<2||second-first>3||dot-second<2||dot-second>3||value.size()-dot!=2)return {};
 unsigned parts[4]{};const std::string fields[]={value.substr(0,first),value.substr(first+1,second-first-1),value.substr(second+1,dot-second-1),value.substr(dot+1)};
 for(unsigned i=0;i<4;++i){const auto& f=fields[i];const auto r=std::from_chars(f.data(),f.data()+f.size(),parts[i],16);if(r.ec!=std::errc{}||r.ptr!=f.data()+f.size())return {};}
 if(parts[1]>255||parts[2]>31||parts[3]>7)return {};
 std::ostringstream s;s<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(8)<<parts[0]<<':'<<std::setw(2)<<parts[1]<<':'<<std::setw(2)<<parts[2]<<'.'<<parts[3];return s.str();
}
struct WindowsTelemetryReading {
 std::string name;
 client::ClientGpuTelemetry values;
 std::string uuid; // Native device UUID; separate from backend/ordinal IDs.
 std::string windowsDeviceInstanceId; // Internal sensor binding; not sent to the server.
};
struct WindowsTelemetryReport {
 bool complete=false;
 std::string detail="GPU telemetry unavailable";
 std::vector<WindowsTelemetryReading> readings;
 // Match physical proofs, never a vendor ordinal or a board-name guess.
 const WindowsTelemetryReading* find(const std::string& requestedUuid,const std::string& requestedPci,
     const std::string& requestedInstance,const std::string& requestedName={})const {
  const auto uuid=canonicalGpuUuid(requestedUuid),pci=canonicalGpuPciBusId(requestedPci),instance=canonicalWindowsGpuInstanceId(requestedInstance);
  if((!requestedUuid.empty()&&uuid.empty())||(!requestedPci.empty()&&pci.empty())||(!requestedInstance.empty()&&instance.empty()))return nullptr;
  const bool hasPhysical=!uuid.empty()||!pci.empty()||!instance.empty();const WindowsTelemetryReading* found=nullptr;
  for(const auto& r:readings){
   const auto ruuid=canonicalGpuUuid(r.uuid),rpci=canonicalGpuPciBusId(r.values.pciBusId),rinstance=canonicalWindowsGpuInstanceId(r.windowsDeviceInstanceId);
   const bool pnpMatch=!instance.empty()&&rinstance==instance;
   const bool related=(!uuid.empty()&&ruuid==uuid)||(!pci.empty()&&rpci==pci)||pnpMatch||
    (!hasPhysical&&!requestedName.empty()&&r.name==requestedName);
   if(!related)continue;
   if((!r.uuid.empty()&&ruuid.empty())||(!r.values.pciBusId.empty()&&rpci.empty())||(!r.windowsDeviceInstanceId.empty()&&rinstance.empty())||
      (!uuid.empty()&&!ruuid.empty()&&uuid!=ruuid)||(!pci.empty()&&!rpci.empty()&&pci!=rpci)||
      (!instance.empty()&&!rinstance.empty()&&instance!=rinstance)||(!requestedName.empty()&&!pnpMatch&&!(hasPhysical&&r.values.vendor=="AMD")&&r.name!=requestedName)||found)return nullptr;
   found=&r;
  }return found;
 }
 bool related(const std::string& requestedUuid,const std::string& requestedPci,const std::string& requestedInstance)const {
  const auto uuid=canonicalGpuUuid(requestedUuid),pci=canonicalGpuPciBusId(requestedPci),instance=canonicalWindowsGpuInstanceId(requestedInstance);
  for(const auto& r:readings)if((!uuid.empty()&&canonicalGpuUuid(r.uuid)==uuid)||(!pci.empty()&&canonicalGpuPciBusId(r.values.pciBusId)==pci)||
    (!instance.empty()&&canonicalWindowsGpuInstanceId(r.windowsDeviceInstanceId)==instance))return true;
  return false;
 }
 bool bindDevice(const std::string& backend,int device,const std::string& uuid,const std::string& pci,const std::string& instance) {
  if(!complete||(backend!="cuda"&&backend!="opencl")||device<0||device>255)return false;
  const auto* selected=find(uuid,pci,instance);if(!selected||!selected->values.valid())return false;
  auto& v=readings[static_cast<std::size_t>(selected-readings.data())].values;
  v.device=device;v.deviceId=backend+'-'+std::to_string(device);return true;
 }
 void bindCudaOrdinals(const std::vector<std::pair<std::string,int>>& devices) {
  for(auto& r:readings){
   unsigned matches=0;int ordinal=-1;
   const auto bus=canonicalGpuPciBusId(r.values.pciBusId);
   if(bus.empty())continue;
   for(const auto& [pci,index]:devices)if(index>=0&&canonicalGpuPciBusId(pci)==bus){++matches;ordinal=index;}
   if(matches==1){r.values.device=ordinal;r.values.deviceId="cuda-"+std::to_string(ordinal);}
  }
 }
 void heartbeat(client::ClientHeartbeat& h)const {
  h.gpuTelemetryReported=complete;h.gpuTelemetry.clear();
  // Incomplete sensor discovery must not replace the last complete inventory.
  if(complete)for(const auto& r:readings)if(r.values.valid())h.gpuTelemetry.push_back(r.values);
 }
};
struct WindowsAmdTelemetryDevice {
 int index=0;std::string name,windowsDeviceInstanceId;
 bool operator==(const WindowsAmdTelemetryDevice&)const=default;
};
struct WindowsAmdTelemetryApi {
 // This boundary contains only reads. Device identity is re-enumerated after
 // sampling; failures never reuse the last sample or invent a temperature.
 std::function<std::vector<WindowsAmdTelemetryDevice>()> devices;
 std::function<std::optional<double>(const std::string&)> temperature;
 std::function<std::optional<double>(const std::string&)> power; // Optional, measured watts only.
};
inline WindowsTelemetryReport queryWindowsAmdTelemetry(const WindowsAmdTelemetryApi& api) {
 WindowsTelemetryReport result;result.detail="AMD ADLX telemetry unavailable";
 try{
  if(!api.devices||!api.temperature)throw std::runtime_error("AMD read-only sensor API incomplete");
  const auto before=api.devices();if(before.size()>256)throw std::runtime_error("AMD inventory exceeds limit");
  std::set<int> indexes;std::set<std::string> instances;
  for(const auto& d:before){
   const auto id=canonicalWindowsGpuInstanceId(d.windowsDeviceInstanceId);
   if(d.index<0||d.index>255||!indexes.insert(d.index).second||id.empty()||!id.starts_with("PCI\\VEN_1002&")||!instances.insert(id).second||
      d.name.empty()||d.name.size()>512||std::any_of(d.name.begin(),d.name.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid or duplicated AMD sensor identity");
   auto value=api.temperature(id);auto power=api.power?api.power(id):std::optional<double>{};
   if(value&&(!std::isfinite(*value)||*value<0||*value>200))value.reset();
   if(power&&(!std::isfinite(*power)||*power<0||*power>10000))power.reset();
   if(!value&&!power)continue;
   WindowsTelemetryReading r;r.name=d.name;r.windowsDeviceInstanceId=id;r.values.vendor="AMD";
   r.values.device=d.index;r.values.deviceId="amd-"+std::to_string(d.index);r.values.temperatureLabel="GPU";r.values.temperatureC=value;r.values.powerDrawW=power;
   // Manual power tuning is a percentage, never a watt limit or temperature.
   result.readings.push_back(std::move(r));
  }
  if(api.devices()!=before)throw std::runtime_error("AMD sensor inventory changed during sampling");
  result.complete=true;result.detail="Read-only AMD ADLX GPU metrics";
 }catch(const std::exception& e){result.complete=false;result.readings.clear();result.detail=e.what();}return result;
}
inline WindowsTelemetryReport mergeWindowsTelemetryReports(const WindowsTelemetryReport& nvidia,const WindowsTelemetryReport& amd,
    bool nvidiaExpected,bool amdExpected) {
 WindowsTelemetryReport result;const bool nv=nvidiaExpected||!nvidia.readings.empty(),am=amdExpected||!amd.readings.empty();
 result.complete=(nv||am)?(!nv||nvidia.complete)&&(!am||amd.complete):(nvidia.complete||amd.complete);
 result.detail="NVIDIA: "+nvidia.detail+"; AMD: "+amd.detail;
 if(nvidia.complete)result.readings=nvidia.readings;
 if(amd.complete)result.readings.insert(result.readings.end(),amd.readings.begin(),amd.readings.end());
 return result;
}
enum class WindowsThermalStatus {Disabled,Normal,Warning,Critical,Unavailable};
struct WindowsRuntimeThermalSettings {
 using Reader=std::function<WindowsTelemetryReport()>;
 GpuThermalPolicyConfiguration policy;
 std::string deviceName;
 Reader reader;
 std::chrono::milliseconds interval{5000};
 std::string devicePciBusId,deviceUuid,deviceWindowsInstanceId;
};
class WindowsThermalMonitor {
 WindowsRuntimeThermalSettings settings_;
 WindowsTelemetryReport report_;
 std::optional<std::chrono::steady_clock::time_point> next_;
 WindowsThermalStatus state_=WindowsThermalStatus::Unavailable;
public:
 explicit WindowsThermalMonitor(WindowsRuntimeThermalSettings settings):settings_(std::move(settings)) {
  if(!GpuThermalPolicy::valid(settings_.policy)||settings_.interval<std::chrono::milliseconds(1))throw std::invalid_argument("Invalid Windows thermal policy");
 }
 void sample(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()) {
  if(next_&&now<*next_)return;
  next_=now+settings_.interval;
  try{report_=settings_.reader?settings_.reader():WindowsTelemetryReport{};}catch(...){report_={};}
  if(!hasSelection()||!settings_.policy.enabled){state_=WindowsThermalStatus::Disabled;return;}
  const auto* selected=report_.complete?report_.find(settings_.deviceUuid,settings_.devicePciBusId,
    settings_.deviceWindowsInstanceId,settings_.deviceName):nullptr;
  if(!selected||!selected->values.valid()||!selected->values.temperatureC){state_=WindowsThermalStatus::Unavailable;return;}
  const auto t=*selected->values.temperatureC;
  if(t>=settings_.policy.criticalC)state_=WindowsThermalStatus::Critical;
  else if(t>=settings_.policy.warningC || (state_==WindowsThermalStatus::Warning&&t>=settings_.policy.warningC-2))state_=WindowsThermalStatus::Warning;
  else state_=WindowsThermalStatus::Normal;
 }
 bool hasSelection()const{return !settings_.deviceName.empty()||!settings_.devicePciBusId.empty()||!settings_.deviceUuid.empty()||!settings_.deviceWindowsInstanceId.empty();}
 bool mustStop()const {return settings_.policy.enabled&&settings_.policy.stopOnCritical&&hasSelection()&&
  (state_==WindowsThermalStatus::Critical||state_==WindowsThermalStatus::Unavailable);}
 WindowsThermalStatus status()const{return state_;}
 const WindowsTelemetryReport& report()const{return report_;}
};
}
