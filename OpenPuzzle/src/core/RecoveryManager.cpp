#include "openpuzzle/core/RecoveryManager.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace openpuzzle {

RecoveryManager::RecoveryManager(WorkspaceManager workspaceManager)
    : workspaceManager_(std::move(workspaceManager)) {}

bool RecoveryManager::hasStateFile(int jobId) const {
    return std::filesystem::exists(workspaceManager_.stateFile(jobId));
}

std::string RecoveryManager::readState(int jobId) const {
    std::ifstream in(workspaceManager_.stateFile(jobId));

    if (!in.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << in.rdbuf();

    return buffer.str();
}

} // namespace openpuzzle
