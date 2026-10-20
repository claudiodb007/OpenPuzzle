#pragma once
#ifdef _WIN32
#include <filesystem>
#include <functional>
#include "openpuzzle/hardware/WindowsGpuDiscovery.hpp"
#include <ostream>
#include <string>
#include <vector>
namespace openpuzzle {
struct WindowsEngineCheckOptions {
  std::string command,backend;
  int device=0,threads=1,blocks=256,points=256,seconds=30;
  bool real=false,threadsSpecified=false,gpuParametersSpecified=false,automatic=false;
  static WindowsEngineCheckOptions parse(const std::vector<std::string>& arguments);
};
// Explicit local diagnostics over direct CreateProcessW launches. No API,
// client identity, registration, assignment or automatic search is involved.
class WindowsEngineDiagnostics {
public:
  explicit WindowsEngineDiagnostics(std::filesystem::path folder={},
    std::function<WindowsGpuInventory()> discovery=WindowsGpuDiscovery::inspect);
  int run(const std::vector<std::string>& arguments,std::ostream& output) const;
  void describe(std::ostream& output) const;
  static std::filesystem::path defaultFolder();
  static std::string executableSha256(const std::filesystem::path& executable);
  static std::string inspectDevice(const std::filesystem::path& executable,
      const std::string& backend,int device,int threads);
private:
  std::filesystem::path folder_;
  std::function<WindowsGpuInventory()> discovery_;
};
}
#endif
