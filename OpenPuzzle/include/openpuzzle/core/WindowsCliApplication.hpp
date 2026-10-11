#pragma once
#ifdef _WIN32
#include "openpuzzle/runtime/WindowsRunSession.hpp"
#include "openpuzzle/hardware/WindowsGpuDiscovery.hpp"
#include <functional>
#include <ostream>
namespace openpuzzle {
// Native local console and explicit CPU/CUDA/OpenCL diagnostics. Assignment
// launch remains provider-gated until native solution/recovery integration is complete.
class WindowsCliApplication {
public:
  explicit WindowsCliApplication(WindowsRunDependencies dependencies={},
    std::function<WindowsGpuInventory()> discovery=WindowsGpuDiscovery::inspect,
    std::filesystem::path engineFolder={});
  int run(const std::vector<std::string>& arguments,std::ostream& output);
private:
  WindowsRunDependencies dependencies_;
  std::filesystem::path engineFolder_;
  std::function<WindowsGpuInventory()> discovery_;
};
}
#endif
