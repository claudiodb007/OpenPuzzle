#include "openpuzzle/runtime/Execution.hpp"

namespace openpuzzle {

Execution::Execution(ExecutionState state)
    : state_(std::move(state)) {}

const ExecutionState& Execution::state() const {
  return state_;
}

ExecutionState& Execution::state() {
  return state_;
}

void Execution::start() {
  state_.status = RuntimeExecutionStatus::Running;
}

void Execution::finish(const ExecutionResult& result) {
  state_.status = RuntimeExecutionStatus::Finished;
  state_.averageSpeed = result.averageSpeed;
  state_.keysChecked = result.keysChecked;
  state_.keyFound = result.keyFound;
  state_.privateKey = result.privateKey;
  state_.exitCode = result.exitCode;
}

void Execution::fail(const ExecutionResult& result) {
  state_.status = RuntimeExecutionStatus::Failed;
  state_.averageSpeed = result.averageSpeed;
  state_.keysChecked = result.keysChecked;
  state_.keyFound = result.keyFound;
  state_.privateKey = result.privateKey;
  state_.exitCode = result.exitCode;
}

void Execution::stop() {
  state_.status = RuntimeExecutionStatus::Stopped;
}

bool Execution::isPending() const {
  return state_.status == RuntimeExecutionStatus::Pending;
}

bool Execution::isRunning() const {
  return state_.status == RuntimeExecutionStatus::Running;
}

bool Execution::isFinished() const {
  return state_.status == RuntimeExecutionStatus::Finished;
}

bool Execution::isFailed() const {
  return state_.status == RuntimeExecutionStatus::Failed;
}

} // namespace openpuzzle
