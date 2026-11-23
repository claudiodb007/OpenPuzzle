#include "openpuzzle/runtime/WindowsExecutionLauncher.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/runtime/WindowsVanityRun.hpp"
#include <stdexcept>
namespace openpuzzle {
std::unique_ptr<WindowsEngineProcess> startWindowsExecution(
    const std::filesystem::path& executable, const std::vector<std::string>& arguments,
    client::ClientExecutionState state, std::optional<WindowsEngineRecord> previous) {
  const auto slot = client::ClientStateStore::executionSlot();
  if (ClientRuntimeControl::runtimePid(slot) != static_cast<int>(GetCurrentProcessId()) ||
      ClientRuntimeControl::runtimeIdentityStatus(slot) != ClientRuntimeControl::RuntimeIdentityStatus::Running)
    throw std::runtime_error("Current Windows runtime does not own this execution slot");
  const auto statePath = client::ClientStateStore::path(slot);
  WorkspaceSecurity::prepare(statePath.parent_path());
  windowsFile::Lock lock(statePath.wstring() + L".execution.lock");
  if (!lock.locked()) throw std::runtime_error("Windows assignment publication is already in progress");
  std::error_code error;
  const auto status = std::filesystem::symlink_status(statePath, error);
  if (previous) {
    const auto old = client::ExecutionSyncService().inspect(slot);
    if (!old.hasState || old.running || old.identityUnavailable || old.solutionFound ||
        old.state.engine != "Vanity" || state.engine != "Vanity" ||
        old.state.assignmentId != state.assignmentId || old.state.clientId != state.clientId ||
        old.state.workspace != state.workspace || windowsExecutionRecord(old.state) != previous)
      throw std::runtime_error("Vanity recovery identity changed; existing state preserved");
  } else if ((error && error != std::errc::no_such_file_or_directory) ||
             std::filesystem::exists(status))
    throw std::runtime_error("Pending or unreadable Windows assignment was preserved");
  const auto workspace = pathFromUtf8(state.workspace);
  if (!workspace.is_absolute() || client::ExecutionSyncService::solutionFile(state.workspace, state.engine))
    throw std::runtime_error("Windows workspace is invalid or contains a preserved solution");
  const auto ownerTime = ClientRuntimeControl::runtimeStartTime(slot);
  if (!ownerTime) throw std::runtime_error("Windows runtime owner identity is unavailable");
  const WindowsRuntimeRecord owner{static_cast<int>(GetCurrentProcessId()), *ownerTime};
  auto commandArguments = arguments;
  std::optional<int> cudaDevice;
  if (state.engine == "Vanity") {
    const auto plan = windowsVanityPlan(state);
    const auto result = windowsVanityResult(state);
    const auto resultStatus = std::filesystem::symlink_status(result, error);
    if ((error && error != std::errc::no_such_file_or_directory) || std::filesystem::exists(resultStatus))
      throw std::runtime_error("Vanity result path already exists; existing evidence preserved");
    commandArguments = {"--target-file", plan.start, std::to_string(plan.count), std::to_string(plan.blocks),
      pathUtf8(workspace / "targets.txt"), pathUtf8(result), "stop", std::to_string(owner.pid)};
    state.command = WindowsCommandLine::build(pathUtf8(executable), commandArguments);
    cudaDevice = state.device;
  }
  // Check metadata before creating any child; bind a temporary valid identity.
  auto checked = state;
  if (!bindWindowsExecutionState(checked, {{1, 1}, "metadata-check"}, owner) || !checked.valid())
    throw std::runtime_error("Windows assignment metadata is invalid");
  return WindowsEngineProcess::start(executable, commandArguments, workspace,
      [state, slot, owner](const WindowsEngineRecord& record) mutable {
        return bindWindowsExecutionState(state, record, owner) && client::ClientStateStore::save(state, slot);
      }, cudaDevice);
}
}
