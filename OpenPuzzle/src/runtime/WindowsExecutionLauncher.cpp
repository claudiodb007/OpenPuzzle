#include "openpuzzle/runtime/WindowsExecutionLauncher.hpp"
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include <stdexcept>
namespace openpuzzle {
std::unique_ptr<WindowsEngineProcess> startWindowsExecution(
    const std::filesystem::path& executable, const std::vector<std::string>& arguments,
    client::ClientExecutionState state) {
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
  if ((error && error != std::errc::no_such_file_or_directory) ||
      std::filesystem::exists(status))
    throw std::runtime_error("Pending or unreadable Windows assignment was preserved");
  const auto workspace = pathFromUtf8(state.workspace);
  if (!workspace.is_absolute() || client::ExecutionSyncService::solutionFile(state.workspace, state.engine))
    throw std::runtime_error("Windows workspace is invalid or contains a preserved solution");
  const auto ownerTime = ClientRuntimeControl::runtimeStartTime(slot);
  if (!ownerTime) throw std::runtime_error("Windows runtime owner identity is unavailable");
  const WindowsRuntimeRecord owner{static_cast<int>(GetCurrentProcessId()), *ownerTime};
  // Check metadata before creating any child; bind a temporary valid identity.
  auto checked = state;
  if (!bindWindowsExecutionState(checked, {{1, 1}, "metadata-check"}, owner) || !checked.valid())
    throw std::runtime_error("Windows assignment metadata is invalid");
  return WindowsEngineProcess::start(executable, arguments, workspace,
      [state, slot, owner](const WindowsEngineRecord& record) mutable {
        return bindWindowsExecutionState(state, record, owner) && client::ClientStateStore::save(state, slot);
      });
}
}
