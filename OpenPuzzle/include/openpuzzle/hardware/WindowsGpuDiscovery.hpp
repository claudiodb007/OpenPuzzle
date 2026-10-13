#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace openpuzzle {

enum class GpuDiscoveryState { Missing, Empty, Available, Error };

struct WindowsGpuDevice {
  int inventoryIndex = 0;
  int platformIndex = -1; // OpenCL platform; CUDA has no platform index.
  int nativeDeviceIndex = 0;
  std::string name, vendor, driverVersion;
  std::uint64_t memoryMiB = 0;
  unsigned int computeUnits = 0;
  bool available = false;
  bool compilerAvailable = false; // OpenCL online compiler; not a kernel test.
  std::string pciBusId; // Optional CUDA PCI proof; never inferred from ordinals.
};

struct WindowsGpuBackend {
  GpuDiscoveryState state = GpuDiscoveryState::Missing;
  std::string detail;
  std::vector<WindowsGpuDevice> devices;
};

struct WindowsGpuInventory {
  unsigned int logicalProcessors = 0;
  std::uint64_t systemMemoryMiB = 0;
  bool nativeCpuAvailable = false;
  WindowsGpuBackend cuda, opencl;
};

// Small, documented x64 driver ABI. No SDK, import library, CUDA context or
// OpenCL context is needed for enumeration. Function tables also permit local
// regression fixtures without requiring the corresponding physical GPU.
struct WindowsCudaDiscoveryApi {
  int (WINAPI *init)(unsigned int) = nullptr;
  int (WINAPI *driverVersion)(int*) = nullptr;
  int (WINAPI *deviceCount)(int*) = nullptr;
  int (WINAPI *deviceGet)(int*, int) = nullptr;
  int (WINAPI *deviceName)(char*, int, int) = nullptr;
  int (WINAPI *deviceMemory)(std::size_t*, int) = nullptr;
  int (WINAPI *deviceAttribute)(int*, int, int) = nullptr;
  int (WINAPI *devicePciBusId)(char*, int, int) = nullptr;
};

struct WindowsOpenClDiscoveryApi {
  int (WINAPI *platforms)(unsigned int, void**, unsigned int*) = nullptr;
  int (WINAPI *devices)(void*, std::uint64_t, unsigned int, void**, unsigned int*) = nullptr;
  int (WINAPI *info)(void*, unsigned int, std::size_t, void*, std::size_t*) = nullptr;
};

class WindowsGpuDiscovery {
public:
  static WindowsGpuInventory inspect();
  static WindowsGpuBackend queryCuda(const WindowsCudaDiscoveryApi& api);
  static WindowsGpuBackend queryOpenCl(const WindowsOpenClDiscoveryApi& api);
  static std::wstring systemLibraryPath(const std::wstring& name);
};

std::string formatWindowsGpuInventory(const WindowsGpuInventory& inventory);
bool windowsGpuDiscoveryHasError(const WindowsGpuInventory& inventory);

} // namespace openpuzzle
#endif
