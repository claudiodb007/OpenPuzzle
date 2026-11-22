#pragma once
#include "openpuzzle/engines/WindowsRunProfile.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include <limits>
namespace openpuzzle {
struct WindowsProfileGpuDevice {
 int platform=0,native=0;
 std::string name,vendor,pciBusId,uuid;
 std::uint64_t memoryMiB=0;
 bool available=false;
 std::string windowsDeviceInstanceId;
 std::string displayName;
};
struct WindowsEngineGpuIdentity {
 std::string name;
 int platform=0,native=0;
 std::uint64_t memoryBytes=0;
 static WindowsEngineGpuIdentity parse(const std::string& value) {
  const auto memory=value.rfind("; memory="),native=value.rfind("; native="),platform=value.rfind("; platform=");
  if(platform==std::string::npos||native==std::string::npos||memory==std::string::npos||!platform||native<=platform+11||memory<=native+9)
   throw std::invalid_argument("Invalid measured engine device mapping");
  WindowsEngineGpuIdentity result;result.name=value.substr(0,platform);
  auto number=[](const std::string& text,std::uint64_t maximum){std::uint64_t n=0;const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
   if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||n>maximum)throw std::invalid_argument("Invalid engine mapping integer");return n;};
  result.platform=static_cast<int>(number(value.substr(platform+11,native-platform-11),63));
  result.native=static_cast<int>(number(value.substr(native+9,memory-native-9),255));
  result.memoryBytes=number(value.substr(memory+9),(std::numeric_limits<std::uint64_t>::max)());
  if(result.memoryBytes<1048576)throw std::invalid_argument("Invalid engine mapping memory");
  return result;
 }
};
struct WindowsPhysicalGpuBinding {std::string name,pciBusId,uuid,windowsDeviceInstanceId,displayName;};
inline WindowsPhysicalGpuBinding resolveWindowsGpuProfileBinding(const WindowsRunProfile& profile,
 const std::vector<WindowsProfileGpuDevice>& devices,const WindowsTelemetryReport& report,bool requireTelemetry=false) {
 if(profile.backend=="cpu")throw std::invalid_argument("CPU profiles do not have a GPU binding");
 const auto engine=WindowsEngineGpuIdentity::parse(profile.deviceIdentity);
 const WindowsProfileGpuDevice* selected=nullptr;unsigned matches=0;
 for(const auto& d:devices)if(d.platform==engine.platform&&d.native==engine.native){selected=&d;++matches;}
 if(matches!=1||!selected->available||selected->name!=engine.name||selected->memoryMiB!=engine.memoryBytes/1048576)
  throw std::runtime_error("Engine mapping does not uniquely match the native driver; profile preserved");
 const auto uuid=canonicalGpuUuid(selected->uuid),bus=canonicalGpuPciBusId(selected->pciBusId);
 const auto instance=canonicalWindowsGpuInstanceId(selected->windowsDeviceInstanceId);
 if((!selected->uuid.empty()&&uuid.empty())||(!selected->pciBusId.empty()&&bus.empty()))
  throw std::runtime_error("Driver supplies a malformed physical GPU identity");
 if(!selected->windowsDeviceInstanceId.empty()&&instance.empty())throw std::runtime_error("Windows supplies a malformed GPU instance identity");
 if(uuid.empty()&&bus.empty()&&instance.empty())throw std::runtime_error("Driver supplies no verified GPU identity (UUID, PCI or Windows device instance)");
 if((!profile.gpuUuid.empty()&&!uuid.empty()&&profile.gpuUuid!=uuid)||
    (!profile.pciBusId.empty()&&!bus.empty()&&profile.pciBusId!=bus)||
    (!profile.windowsDeviceInstanceId.empty()&&profile.windowsDeviceInstanceId!=instance))
  throw std::runtime_error("Physical GPU differs from the bound profile; no ordinal fallback");
 for(const auto& d:devices)if(&d!=selected&&((!uuid.empty()&&canonicalGpuUuid(d.uuid)==uuid)||(!bus.empty()&&canonicalGpuPciBusId(d.pciBusId)==bus)||
    (!instance.empty()&&canonicalWindowsGpuInstanceId(d.windowsDeviceInstanceId)==instance)))
  throw std::runtime_error("Driver reports a duplicated physical GPU identity");
 const auto* sensor=report.complete?report.find(uuid,bus,instance,engine.name):nullptr;
 const bool sensorEvidence=report.complete&&report.related(uuid,bus,instance);
 // Physical driver identity is sufficient to bind a measured profile. Sensors
 // are a separate capability: AMD/OpenCL and NVIDIA without NVML can search
 // when the configured policy does not require critical-temperature stopping.
 // Available NVIDIA readings still cross-check the mapping; never ignore a
 // contradictory, partial or duplicated sensor identity.
 const bool nvidia=selected->vendor=="NVIDIA"||selected->vendor=="NVIDIA Corporation";
 const bool checkSensor=requireTelemetry||sensorEvidence||(nvidia&&report.complete&&(!uuid.empty()||!bus.empty()));
 std::string confirmedUuid=uuid,confirmedPci=bus;
 if(checkSensor){
  if(!sensor||!sensor->values.valid()||
     (!bus.empty()&&canonicalGpuPciBusId(sensor->values.pciBusId)!=bus)||
     (!uuid.empty()&&!sensor->uuid.empty()&&canonicalGpuUuid(sensor->uuid)!=uuid)||
     (requireTelemetry&&!sensor->values.temperatureC))
   throw std::runtime_error("Telemetry does not uniquely confirm the engine-selected physical GPU");
  if(const auto value=canonicalGpuUuid(sensor->uuid);!value.empty())confirmedUuid=value;
  if(const auto value=canonicalGpuPciBusId(sensor->values.pciBusId);!value.empty())confirmedPci=value;
 }
 if((!profile.gpuUuid.empty()&&profile.gpuUuid!=confirmedUuid)||(!profile.pciBusId.empty()&&profile.pciBusId!=confirmedPci))
  throw std::runtime_error("Physical identity cannot confirm the bound profile");
 if(checkSensor&&!confirmedUuid.empty())for(const auto& r:report.readings)if(&r!=sensor&&canonicalGpuUuid(r.uuid)==confirmedUuid)
  throw std::runtime_error("Telemetry reports a duplicated confirmed UUID");
 // Do not make standard UUID/PCI profiles depend on optional Windows metadata.
 const bool useInstance=(!profile.windowsDeviceInstanceId.empty()||(confirmedUuid.empty()&&confirmedPci.empty()));
 return {engine.name,confirmedPci,confirmedUuid,useInstance?instance:std::string{},selected->displayName};
}
inline std::string appendWindowsGpuProfileBinding(const std::string& original,const WindowsPhysicalGpuBinding& binding) {
 const auto profile=WindowsRunProfile::parse(original);
 if(profile.hasPhysicalBinding())throw std::invalid_argument("Profile already contains physical identity; original preserved");
 const auto uuid=canonicalGpuUuid(binding.uuid),pci=canonicalGpuPciBusId(binding.pciBusId);
 const auto instance=canonicalWindowsGpuInstanceId(binding.windowsDeviceInstanceId);
 if(profile.backend=="cpu"||(uuid.empty()&&pci.empty()&&instance.empty())||(!binding.uuid.empty()&&uuid.empty())||(!binding.pciBusId.empty()&&pci.empty())||
    (!binding.windowsDeviceInstanceId.empty()&&instance.empty()))
  throw std::invalid_argument("Invalid new physical GPU binding");
 const auto end=original.find_last_not_of(" \t\r\n");
 if(end==std::string::npos||original[end]!='}')throw std::invalid_argument("Profile is not a complete JSON object");
 std::string result=original.substr(0,end);
 if(!uuid.empty())result+=",\"gpu_uuid\":\""+uuid+"\"";
 if(!pci.empty())result+=",\"pci_bus_id\":\""+pci+"\"";
 if(!instance.empty())result+=",\"windows_device_instance_id\":\""+escapeJsonString(instance)+"\"";
 result+='}';result+=original.substr(end+1);
 (void)WindowsRunProfile::parse(result);return result;
}
}
