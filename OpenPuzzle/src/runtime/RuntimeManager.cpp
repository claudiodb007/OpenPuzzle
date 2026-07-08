#include "openpuzzle/runtime/RuntimeManager.hpp"

namespace openpuzzle {

RuntimeManager::RuntimeManager() = default;

ExecutionResult RuntimeManager::run(const ExecutionContext& context,
                                    int maxSeconds,
                                    int maxSamples) {
  ExecutionState state;
  state.executionId = context.executionId;
  state.puzzleId = context.puzzleId;
  state.jobId = context.jobId;
  state.rangeId = context.rangeId;
  state.engine = context.engine;
  state.workspace = context.workspace;
  state.command = context.command;
  state.status = RuntimeExecutionStatus::Running;

  activeExecutions_.push_back(state);

  auto result = executionManager_.run(context, maxSeconds, maxSamples);

  activeExecutions_.back().status =
      result.success ? RuntimeExecutionStatus::Finished
                     : RuntimeExecutionStatus::Failed;
  activeExecutions_.back().averageSpeed = result.averageSpeed;
  activeExecutions_.back().keysChecked = result.keysChecked;
  activeExecutions_.back().keyFound = result.keyFound;
  activeExecutions_.back().privateKey = result.privateKey;
  activeExecutions_.back().exitCode = result.exitCode;

  return result;
}

const std::vector<ExecutionState>& RuntimeManager::activeExecutions() const {
  return activeExecutions_;
}

} // namespace openpuzzle
