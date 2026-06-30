#pragma once

#include "openpuzzle/core/RecoveryState.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

namespace openpuzzle {

class RecoveryManager {
public:
  explicit RecoveryManager(WorkspaceManager workspaceManager);

  bool hasStateFile(int jobId) const;

  RecoveryState load(int jobId) const;

private:
  WorkspaceManager workspaceManager_;
};

} // namespace openpuzzle
