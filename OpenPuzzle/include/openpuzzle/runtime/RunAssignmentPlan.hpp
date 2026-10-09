#pragma once
#include "openpuzzle/runtime/RunCommandOptions.hpp"
#include "openpuzzle/client/RangeAssignment.hpp"
#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/engines/common/PuzzleExecutionPlanner.hpp"
#include <filesystem>
namespace openpuzzle {
struct RunAssignmentPlan {
  client::ClientExecutionState state;
  std::vector<std::string> arguments;
  std::string targetFileContent;
  static bool safeAssignmentId(const std::string& id);
  // Pure validation/planning: never creates files, launches processes or claims work.
  static RunAssignmentPlan build(const RunCommandOptions& options,
      const client::RangeAssignment& assignment,const std::string& clientId,
      const std::filesystem::path& workspace,const PuzzleExecutionMetadata* metadata=nullptr);
};
}
