#include "openpuzzle/core/RecoveryManager.hpp"

#include <filesystem>

namespace openpuzzle {

RecoveryManager::RecoveryManager(WorkspaceManager workspaceManager)
    : workspaceManager_(std::move(workspaceManager))
{
}

bool RecoveryManager::hasStateFile(int jobId) const
{
    return std::filesystem::exists(workspaceManager_.stateFile(jobId));
}

RecoveryState RecoveryManager::load(int jobId) const
{
    (void)jobId;

    // Parser será implementado mais à frente.
    return {};
}

} // namespace openpuzzle
