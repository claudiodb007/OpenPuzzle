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

  registry_.add(state);

  auto result = executionManager_.run(context, maxSeconds, maxSamples);

  state.status = result.success ? RuntimeExecutionStatus::Finished
                                : RuntimeExecutionStatus::Failed;
  state.averageSpeed = result.averageSpeed;
  state.keysChecked = result.keysChecked;
  state.keyFound = result.keyFound;
  state.privateKey = result.privateKey;
  state.exitCode = result.exitCode;

  registry_.update(state);

  return result;
}

std::vector<ExecutionState> RuntimeManager::activeExecutions() const {
  return registry_.active();
}

const ExecutionRegistry& RuntimeManager::registry() const {
  return registry_;
}

} // namespace openpuzzle
