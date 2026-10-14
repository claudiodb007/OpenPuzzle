#pragma once
#ifdef _WIN32
#include "openpuzzle/engines/WindowsGpuProfileBinding.hpp"
#include "openpuzzle/hardware/WindowsGpuDiscovery.hpp"
#include "openpuzzle/hardware/WindowsGpuTelemetry.hpp"
#include <filesystem>
#include <ostream>
namespace openpuzzle {
using WindowsGpuInventoryReader=std::function<WindowsGpuInventory()>;
WindowsPhysicalGpuBinding inspectWindowsProfileGpuBinding(const WindowsRunProfile& profile,
 const std::filesystem::path& executable,
 const WindowsGpuInventoryReader& discovery=WindowsGpuDiscovery::inspect,
 const WindowsRuntimeThermalSettings::Reader& telemetry=WindowsGpuTelemetry::inspect);
int bindWindowsGpuProfile(const std::vector<std::string>& arguments,const std::filesystem::path& engineFolder,
 const WindowsGpuInventoryReader& discovery,const WindowsRuntimeThermalSettings::Reader& telemetry,std::ostream& output);
}
#endif
