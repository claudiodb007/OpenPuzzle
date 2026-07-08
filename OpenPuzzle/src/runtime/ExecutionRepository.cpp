#include "openpuzzle/runtime/ExecutionRepository.hpp"

namespace openpuzzle {

ExecutionRepository::ExecutionRepository(Database& database)
    : database_(database) {}

int ExecutionRepository::create(int jobId,
                                const std::string& workspace,
                                const std::string& command,
                                const std::string& state) {
  return database_.insertExecution(jobId, workspace, command, state);
}

bool ExecutionRepository::finish(const Execution& execution) {
  const auto& state = execution.state();

  std::string status = "failed";

  if (execution.isFinished()) {
    status = "finished";
  } else if (execution.isFailed()) {
    status = "failed";
  } else if (execution.isRunning()) {
    status = "running";
  }

  return database_.finishExecution(state.executionId, status, state.exitCode);
}

} // namespace openpuzzle
