#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace openpuzzle {

struct KangarooCheckpointRecoveryPlan {
  bool eligible = false;
  std::string error;
  std::string walkSeed;
  std::uint64_t generation = 0;
  StartExecutionRequest request;
};

class KangarooCheckpointRecoveryPlanner {
public:
  static KangarooCheckpointRecoveryPlan build(
      const client::ClientExecutionState &state,
      const std::filesystem::path &executable,
      const std::filesystem::path &expectedWorkspace);
};

} // namespace openpuzzle
