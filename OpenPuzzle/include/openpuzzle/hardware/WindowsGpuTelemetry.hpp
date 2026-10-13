#pragma once
#include "openpuzzle/hardware/WindowsTelemetryModel.hpp"
#ifdef _WIN32
#include <windows.h>
namespace openpuzzle {
// Read-only NVML ABI, documented at docs.nvidia.com/deploy/nvml-api.
struct WindowsNvmlPciInfo {
 char busIdLegacy[16];
 unsigned int domain,bus,device,pciDeviceId,pciSubSystemId;
 char busId[32];
};
struct WindowsNvmlApi {
 int (*init)()=nullptr;
 int (*shutdown)()=nullptr;
 int (*count)(unsigned int*)=nullptr;
 int (*handle)(unsigned int,void**)=nullptr;
 int (*name)(void*,char*,unsigned int)=nullptr;
 int (*pci)(void*,WindowsNvmlPciInfo*)=nullptr;
 int (*temperature)(void*,unsigned int,unsigned int*)=nullptr;
 int (*power)(void*,unsigned int*)=nullptr;
 int (*limit)(void*,unsigned int*)=nullptr;
};
class WindowsGpuTelemetry {
public:
 static WindowsTelemetryReport inspect();
 static WindowsTelemetryReport query(const WindowsNvmlApi& api);
 static std::wstring systemNvmlPath();
};
std::string formatWindowsTelemetry(const WindowsTelemetryReport&,const GpuThermalPolicyConfiguration& policy={});
}
#endif
