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
struct WindowsPhysicalGpuBinding {std::string name,pciBusId,uuid;};
inline WindowsPhysicalGpuBinding resolveWindowsGpuProfileBinding(const WindowsRunProfile& profile,
 const std::vector<WindowsProfileGpuDevice>& devices,const WindowsTelemetryReport& report) {
 if(profile.backend=="cpu")throw std::invalid_argument("CPU profiles do not have a GPU binding");
 const auto engine=WindowsEngineGpuIdentity::parse(profile.deviceIdentity);
 const WindowsProfileGpuDevice* selected=nullptr;unsigned matches=0;
 for(const auto& d:devices)if(d.platform==engine.platform&&d.native==engine.native){selected=&d;++matches;}
 if(matches!=1||!selected->available||selected->name!=engine.name||selected->memoryMiB!=engine.memoryBytes/1048576)
  throw std::runtime_error("Engine mapping does not uniquely match the native driver; profile preserved");
 if(selected->vendor!="NVIDIA"&&selected->vendor!="NVIDIA Corporation")throw std::runtime_error("Physical thermal binding currently requires NVIDIA");
 const auto uuid=canonicalGpuUuid(selected->uuid),bus=canonicalGpuPciBusId(selected->pciBusId);
 if(uuid.empty()&&bus.empty())throw std::runtime_error("Driver supplies neither a verified UUID nor PCI identity");
 if((!profile.gpuUuid.empty()&&!uuid.empty()&&profile.gpuUuid!=uuid)||
    (!profile.pciBusId.empty()&&!bus.empty()&&profile.pciBusId!=bus))
  throw std::runtime_error("Physical GPU differs from the bound profile; no ordinal fallback");
 for(const auto& d:devices)if(&d!=selected&&((!uuid.empty()&&canonicalGpuUuid(d.uuid)==uuid)||(!bus.empty()&&canonicalGpuPciBusId(d.pciBusId)==bus)))
  throw std::runtime_error("Driver reports a duplicated physical GPU identity");
 const WindowsTelemetryReading* sensor=nullptr;matches=0;
 if(report.complete)for(const auto& r:report.readings){
  const bool same=!uuid.empty()?canonicalGpuUuid(r.uuid)==uuid:canonicalGpuPciBusId(r.values.pciBusId)==bus;
  if(same){sensor=&r;++matches;}
 }
 if(matches!=1||sensor->name!=engine.name||!sensor->values.valid()||
    (!bus.empty()&&canonicalGpuPciBusId(sensor->values.pciBusId)!=bus))
  throw std::runtime_error("NVML does not uniquely confirm the engine-selected physical GPU");
 const auto confirmedUuid=canonicalGpuUuid(sensor->uuid),confirmedPci=canonicalGpuPciBusId(sensor->values.pciBusId);
 if((!profile.gpuUuid.empty()&&profile.gpuUuid!=confirmedUuid)||(!profile.pciBusId.empty()&&profile.pciBusId!=confirmedPci))
  throw std::runtime_error("NVML physical identity differs from the bound profile");
 if(!confirmedUuid.empty())for(const auto& r:report.readings)if(&r!=sensor&&canonicalGpuUuid(r.uuid)==confirmedUuid)
  throw std::runtime_error("NVML reports a duplicated confirmed UUID");
 return {engine.name,confirmedPci,confirmedUuid};
}
inline std::string appendWindowsGpuProfileBinding(const std::string& original,const WindowsPhysicalGpuBinding& binding) {
 const auto profile=WindowsRunProfile::parse(original);
 if(!profile.gpuUuid.empty()||!profile.pciBusId.empty())throw std::invalid_argument("Profile already contains physical identity; original preserved");
 const auto uuid=canonicalGpuUuid(binding.uuid),pci=canonicalGpuPciBusId(binding.pciBusId);
 if(profile.backend=="cpu"||(uuid.empty()&&pci.empty())||(!binding.uuid.empty()&&uuid.empty())||(!binding.pciBusId.empty()&&pci.empty()))
  throw std::invalid_argument("Invalid new physical GPU binding");
 const auto end=original.find_last_not_of(" \t\r\n");
 if(end==std::string::npos||original[end]!='}')throw std::invalid_argument("Profile is not a complete JSON object");
 std::string result=original.substr(0,end);
 if(!uuid.empty())result+=",\"gpu_uuid\":\""+uuid+"\"";
 if(!pci.empty())result+=",\"pci_bus_id\":\""+pci+"\"";
 result+='}';result+=original.substr(end+1);
 (void)WindowsRunProfile::parse(result);return result;
}
}
