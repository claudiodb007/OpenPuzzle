#pragma once
#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#include "openpuzzle/client/ClientExecutionState.hpp"
namespace openpuzzle {
// The caller must own the current runtime slot. Runs one already assigned job;
// it never claims another range and refuses any pending or unreadable local state.
std::unique_ptr<WindowsEngineProcess> startWindowsExecution(
    const std::filesystem::path& executable, const std::vector<std::string>& arguments,
    client::ClientExecutionState state);
}
