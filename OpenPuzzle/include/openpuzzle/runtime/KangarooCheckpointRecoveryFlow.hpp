#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <filesystem>
#include <string>

namespace openpuzzle {

enum class KangarooCheckpointRecoveryFlowStatus {
  Resumed,
  Retry,
  Rejected,
  Invalid
};

struct KangarooCheckpointRecoveryFlowResult {
  KangarooCheckpointRecoveryFlowStatus status =
      KangarooCheckpointRecoveryFlowStatus::Invalid;
  client::ClientExecutionState state;
  std::string error;

  bool resumed() const {
    return status ==
           KangarooCheckpointRecoveryFlowStatus::Resumed;
  }
};

class KangarooCheckpointRecoveryFlow {
public:
  static KangarooCheckpointRecoveryFlowResult recover(
      const std::string &serverUrl,
      const client::ClientExecutionState &state,
      const std::filesystem::path &executable,
      const std::filesystem::path &expectedWorkspace);
};

} // namespace openpuzzle
