#include "openpuzzle/runtime/WindowsProfileGpuBinding.hpp"
#include "openpuzzle/engines/WindowsEngineDiagnostics.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/AtomicFile.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include <set>
namespace openpuzzle {
namespace fs=std::filesystem;
namespace {
WindowsPhysicalGpuBinding snapshot(const WindowsRunProfile& p,const WindowsGpuInventoryReader& discovery,
 const WindowsRuntimeThermalSettings::Reader& telemetry) {
 if(!discovery||!telemetry)throw std::runtime_error("Physical GPU discovery provider unavailable");
 const auto inventory=discovery();const auto& backend=p.backend=="cuda"?inventory.cuda:inventory.opencl;
 if(backend.state!=GpuDiscoveryState::Available)throw std::runtime_error("Selected backend driver inventory unavailable");
 std::vector<WindowsProfileGpuDevice> devices;
 for(const auto& d:backend.devices)devices.push_back({p.backend=="cuda"?0:d.platformIndex,d.nativeDeviceIndex,
  d.name,d.vendor,d.pciBusId,d.uuid,d.memoryMiB,d.available});
 return resolveWindowsGpuProfileBinding(p,devices,telemetry());
}
bool same(const WindowsPhysicalGpuBinding& a,const WindowsPhysicalGpuBinding& b){return a.name==b.name&&a.uuid==b.uuid&&a.pciBusId==b.pciBusId;}
}
WindowsPhysicalGpuBinding inspectWindowsProfileGpuBinding(const WindowsRunProfile& profile,const fs::path& executable,
 const WindowsGpuInventoryReader& discovery,const WindowsRuntimeThermalSettings::Reader& telemetry) {
 if(WindowsEngineDiagnostics::executableSha256(executable)!=profile.sha256)throw std::runtime_error("Profile engine fingerprint changed");
 const auto engine=WindowsEngineDiagnostics::inspectDevice(executable,profile.backend,profile.device,profile.threads);
 if(engine!=profile.deviceIdentity)throw std::runtime_error("Measured engine mapping changed; profile preserved");
 const auto first=snapshot(profile,discovery,telemetry);
 if(WindowsEngineDiagnostics::inspectDevice(executable,profile.backend,profile.device,profile.threads)!=engine||
    !same(first,snapshot(profile,discovery,telemetry))||WindowsEngineDiagnostics::executableSha256(executable)!=profile.sha256)
  throw std::runtime_error("Physical device inventory changed during verification; retry required");
 return first;
}
int bindWindowsGpuProfile(const std::vector<std::string>& args,const fs::path& engineFolder,
 const WindowsGpuInventoryReader& discovery,const WindowsRuntimeThermalSettings::Reader& telemetry,std::ostream& out) {
 std::string source,destination;std::set<std::string> seen;
 for(std::size_t i=1;i<args.size();++i){const auto& option=args[i];
  if((option!="--profile"&&option!="--output")||!seen.insert(option).second||++i==args.size())throw std::invalid_argument("bind-profile requires --profile FILE and --output NEW_FILE");
  (option=="--profile"?source:destination)=args[i];
 }
 if(source.empty()||destination.empty())throw std::invalid_argument("bind-profile requires --profile FILE and --output NEW_FILE");
 const auto input=pathFromUtf8(source),output=pathFromUtf8(destination);
 if(!input.is_absolute()||!output.is_absolute()||input.lexically_normal()==output.lexically_normal()||
    !windowsFile::safePath(input)||!windowsFile::safePath(output))throw std::invalid_argument("Use distinct absolute safe profile paths");
 std::error_code error;const auto status=fs::symlink_status(output,error);
 if((error&&error!=std::errc::no_such_file_or_directory)||fs::exists(status))throw std::runtime_error("Output profile already exists or is unavailable; preserved");
 bool missing=false;const auto contents=windowsFile::readPrivate(input,missing,16384);
 if(!contents)throw std::runtime_error("Measured profile is missing or unreadable; preserved");
 const auto profile=WindowsRunProfile::parse(*contents);
 if(profile.backend=="cpu")throw std::invalid_argument("KeyHunt CPU profiles have no physical GPU binding");
 if(!profile.gpuUuid.empty()||!profile.pciBusId.empty())throw std::invalid_argument("Profile is already physically bound; preserved");
 const auto folder=engineFolder.empty()?WindowsEngineDiagnostics::defaultFolder():engineFolder;
 const auto executable=folder/(profile.backend=="cuda"?"cuBitCrack.exe":"clBitCrack.exe");
 const auto binding=inspectWindowsProfileGpuBinding(profile,executable,discovery,telemetry);
 const auto record=appendWindowsGpuProfileBinding(*contents,binding);
 // Explicit copy operation only; never changes the measured input profile.
 WorkspaceSecurity::prepare(output.parent_path());windowsFile::Lock lock(output.wstring()+L".binding.lock");
 if(!lock.locked())throw std::runtime_error("New profile publication lock unavailable");
 error.clear();const auto current=fs::symlink_status(output,error);
 if((error&&error!=std::errc::no_such_file_or_directory)||fs::exists(current))throw std::runtime_error("Output profile appeared; preserved");
 const auto unchanged=windowsFile::readPrivate(input,missing,16384);
 if(!unchanged||*unchanged!=*contents)throw std::runtime_error("Source profile changed; no bound copy published");
 if(!writePrivateFileAtomically(output,record))throw std::runtime_error("Bound profile could not be published");
 out<<"Physical GPU profile: bound\nGPU: "<<binding.name<<"\nGPU UUID: "<<(binding.uuid.empty()?"unavailable":binding.uuid)
    <<"\nPCI: "<<(binding.pciBusId.empty()?"unavailable":binding.pciBusId)<<"\nOutput profile: "<<pathUtf8(output)
    <<"\nMeasured speed and parameters: preserved\nOriginal profile: preserved\nNo benchmark or production request performed\n";return 0;
}
}
