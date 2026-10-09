#pragma once
#include "openpuzzle/runtime/WindowsEngineRecord.hpp"
#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace openpuzzle {
// An owned Windows engine tree. No shell, detached orphan, production API or
// automatic range allocation. The supervising owner must remain alive.
class WindowsEngineProcess {
public:
  struct Recovery {
    WindowsProcessIdentity::Status status=WindowsProcessIdentity::Status::Unavailable;
    std::optional<WindowsEngineRecord> record;
    std::optional<std::uint32_t> exitCode;
    bool completionUnavailable=false;
  };
  static std::unique_ptr<WindowsEngineProcess> start(const std::filesystem::path& executable,
      const std::vector<std::string>& arguments,const std::filesystem::path& workspace,
      const std::function<bool(const WindowsEngineRecord&)>& publishState = {});
  static Recovery recover(const std::filesystem::path& workspace);
  const WindowsEngineRecord& identity()const;
  std::filesystem::path logPath()const;
  static std::filesystem::path identityPath(const std::filesystem::path& workspace);
  static std::filesystem::path generationPath(const std::filesystem::path& workspace,const WindowsEngineRecord& record);
  // Empty means still running. The full DWORD exit value is retained (including 259).
  std::optional<std::uint32_t> wait(std::uint32_t milliseconds=0);
  std::uint32_t stop(std::uint32_t exitCode=124);
  ~WindowsEngineProcess();
  WindowsEngineProcess(const WindowsEngineProcess&)=delete;
  WindowsEngineProcess& operator=(const WindowsEngineProcess&)=delete;
private:
  struct Impl;
  explicit WindowsEngineProcess(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};
// Runs one owned engine and polls the existing identity-bound runtime controls.
// Safe stop waits for natural completion; stop terminates only this owned Job.
struct WindowsEngineSessionResult {
  std::uint32_t exitCode=0;
  bool safeStopRequested=false;
  bool stopRequested=false;
  bool timedOut=false;
};
WindowsEngineSessionResult runWindowsEngineInSlot(const std::filesystem::path& executable,
    const std::vector<std::string>& arguments,const std::filesystem::path& workspace,
    std::uint32_t timeoutMilliseconds=0);
}
