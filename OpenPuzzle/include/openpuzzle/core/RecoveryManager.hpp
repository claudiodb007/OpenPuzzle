#pragma once

#include "openpuzzle/core/WorkspaceManager.hpp"

#include <string>

namespace openpuzzle {

class RecoveryManager {
public:
    explicit RecoveryManager(WorkspaceManager workspaceManager);

    bool hasStateFile(int jobId) const;
    std::string readState(int jobId) const;

private:
    WorkspaceManager workspaceManager_;
};

} // namespace openpuzzle
