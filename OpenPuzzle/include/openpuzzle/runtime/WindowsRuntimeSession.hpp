#pragma once
#ifdef _WIN32
#include "openpuzzle/runtime/ClientRuntime.hpp"
#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#include "openpuzzle/hardware/WindowsTelemetryModel.hpp"
#include <filesystem>
#include <functional>
#include <memory>
namespace openpuzzle {
// Native adapter for the existing continuous loop and assignment monitor.
// The caller supplies verified capabilities and a preparation callback; GPU
// discovery, command planning and production allocation are not implemented here.
class WindowsRuntimeSession {
public:
  WindowsRuntimeSession(std::string serverUrl, client::ClientHeartbeat capabilities,
                        std::function<bool()> preflight, WindowsRuntimeThermalSettings thermal={});
  int runContinuous(const std::function<ClientIterationResult(WindowsRuntimeSession&)>& prepare, bool once=false,
      const std::function<ClientIterationResult()>& initialize={});
  ClientIterationResult execute(const std::filesystem::path& executable,
                               const std::vector<std::string>& arguments,
                               client::ClientExecutionState state);
  bool thermalReady(){return !pollThermal();}
  ~WindowsRuntimeSession();
  WindowsRuntimeSession(const WindowsRuntimeSession&)=delete;
  WindowsRuntimeSession& operator=(const WindowsRuntimeSession&)=delete;
private:
  std::string serverUrl_, slot_;
  client::ClientHeartbeat capabilities_;
  std::function<bool()> preflight_;
  WindowsThermalMonitor thermal_;
  bool thermalStop_=false;
  std::optional<WindowsThermalStatus> lastThermal_;
  std::unique_ptr<WindowsEngineProcess> engine_;
  client::ClientExecutionState state_;
  bool completed_=false, wakePending_=false, solutionWakeConsumed_=false, awaitingProgress_=false;
  ClientRuntimeDependencies dependencies();
  bool hasState() const;
  bool stopped() const;
  void poll();
  bool pollThermal();
  bool stopOwned(const std::string& workspace);
  void closeOwned() noexcept;
};
}
#endif
