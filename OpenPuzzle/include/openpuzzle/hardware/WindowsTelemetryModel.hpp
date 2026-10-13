#pragma once
#include "openpuzzle/client/ClientHeartbeat.hpp"
#include "openpuzzle/hardware/GpuThermalPolicy.hpp"
#include <chrono>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <functional>
#include <stdexcept>
#include <vector>
namespace openpuzzle {
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
};
struct WindowsTelemetryReport {
 bool complete=false;
 std::string detail="NVIDIA telemetry unavailable";
 std::vector<WindowsTelemetryReading> readings;
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
enum class WindowsThermalStatus {Disabled,Normal,Warning,Critical,Unavailable};
struct WindowsRuntimeThermalSettings {
 using Reader=std::function<WindowsTelemetryReport()>;
 GpuThermalPolicyConfiguration policy;
 std::string deviceName;
 Reader reader;
 std::chrono::milliseconds interval{5000};
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
  if(settings_.deviceName.empty()||!settings_.policy.enabled){state_=WindowsThermalStatus::Disabled;return;}
  const WindowsTelemetryReading* selected=nullptr;unsigned matches=0;
  if(report_.complete)for(const auto& r:report_.readings)if(r.name==settings_.deviceName){selected=&r;++matches;}
  if(matches!=1||!selected->values.valid()||!selected->values.temperatureC){state_=WindowsThermalStatus::Unavailable;return;}
  const auto t=*selected->values.temperatureC;
  if(t>=settings_.policy.criticalC)state_=WindowsThermalStatus::Critical;
  else if(t>=settings_.policy.warningC || (state_==WindowsThermalStatus::Warning&&t>=settings_.policy.warningC-2))state_=WindowsThermalStatus::Warning;
  else state_=WindowsThermalStatus::Normal;
 }
 bool mustStop()const {return settings_.policy.enabled&&settings_.policy.stopOnCritical&&!settings_.deviceName.empty()&&
  (state_==WindowsThermalStatus::Critical||state_==WindowsThermalStatus::Unavailable);}
 WindowsThermalStatus status()const{return state_;}
 const WindowsTelemetryReport& report()const{return report_;}
};
}
