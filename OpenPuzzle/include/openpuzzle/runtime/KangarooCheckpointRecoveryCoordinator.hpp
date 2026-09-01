#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/runtime/KangarooCheckpointRecoveryPlanner.hpp"
#include "openpuzzle/runtime/KangarooRecoveryLeaseGuard.hpp"

#include <string>

namespace openpuzzle {

struct KangarooCheckpointRecoveryResult {
  bool resumed = false;
  client::ClientExecutionState state;
  std::string error;
};

class KangarooCheckpointRecoveryCoordinator {
public:
  static KangarooCheckpointRecoveryResult resume(
      const client::ClientExecutionState &previousState,
      const KangarooCheckpointRecoveryPlan &plan,
      const KangarooRecoveryLeaseResult &lease);
};

} // namespace openpuzzle
