#pragma once
#ifdef _WIN32
#include "openpuzzle/runtime/RunAssignmentPlan.hpp"
#include "openpuzzle/runtime/WindowsRuntimeSession.hpp"
#include <optional>
#include <ostream>
namespace openpuzzle {
struct WindowsRunEngine {
  std::string engine,backend;
  std::filesystem::path executable;
  std::vector<int> devices;
  double speedMKeys=0;
  bool verified=false;
};
struct WindowsRunDependencies {
  std::string serverUrl;
  std::vector<WindowsRunEngine> engines;
  std::function<client::ClientHeartbeat(const std::string&)> capabilities;
  std::function<bool(const RunCommandOptions&,const WindowsRunEngine&)> preflight;
  std::function<std::optional<PuzzleExecutionMetadata>(int)> metadata;
};
// Native command orchestration over the validated runtime. Verified bindings and
// hardware/profile preflight are supplied by the platform provider. This is not
// an installer, GPU discovery implementation or a standalone application entry.
class WindowsRunSession {
public:
  explicit WindowsRunSession(WindowsRunDependencies dependencies);
  int run(const std::vector<std::string>& arguments,std::ostream& output);
  static std::filesystem::path pendingClaimPath(const std::string& slot);
private:
  WindowsRunDependencies dependencies_;
};
}
#endif
