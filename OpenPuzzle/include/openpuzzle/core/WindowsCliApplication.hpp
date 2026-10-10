#pragma once
#ifdef _WIN32
#include "openpuzzle/runtime/WindowsRunSession.hpp"
#include "openpuzzle/hardware/WindowsGpuDiscovery.hpp"
#include <functional>
#include <ostream>
namespace openpuzzle {
// Local Windows console entry. Production engine discovery/profile verification
// remains a provider dependency; the standalone executable supplies no bindings.
class WindowsCliApplication {
public:
  explicit WindowsCliApplication(WindowsRunDependencies dependencies={},
    std::function<WindowsGpuInventory()> discovery=WindowsGpuDiscovery::inspect);
  int run(const std::vector<std::string>& arguments,std::ostream& output);
private:
  WindowsRunDependencies dependencies_;
  std::function<WindowsGpuInventory()> discovery_;
};
}
#endif
