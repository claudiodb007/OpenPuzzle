#include "openpuzzle/runtime/ExecutionRegistry.hpp"

namespace openpuzzle {

void ExecutionRegistry::add(const ExecutionState& state) {
  executions_.push_back(state);
}

std::optional<ExecutionState> ExecutionRegistry::find(int executionId) const {
  for (const auto& execution : executions_) {
    if (execution.executionId == executionId) {
      return execution;
    }
  }

  return std::nullopt;
}

std::vector<ExecutionState> ExecutionRegistry::all() const {
  return executions_;
}

std::vector<ExecutionState> ExecutionRegistry::active() const {
  std::vector<ExecutionState> result;

  for (const auto& execution : executions_) {
    if (execution.status == RuntimeExecutionStatus::Pending ||
        execution.status == RuntimeExecutionStatus::Running) {
      result.push_back(execution);
    }
  }

  return result;
}

std::vector<ExecutionState> ExecutionRegistry::finished() const {
  std::vector<ExecutionState> result;

  for (const auto& execution : executions_) {
    if (execution.status == RuntimeExecutionStatus::Finished) {
      result.push_back(execution);
    }
  }

  return result;
}

std::vector<ExecutionState> ExecutionRegistry::failed() const {
  std::vector<ExecutionState> result;

  for (const auto& execution : executions_) {
    if (execution.status == RuntimeExecutionStatus::Failed) {
      result.push_back(execution);
    }
  }

  return result;
}

bool ExecutionRegistry::update(const ExecutionState& state) {
  for (auto& execution : executions_) {
    if (execution.executionId == state.executionId) {
      execution = state;
      return true;
    }
  }

  return false;
}

std::size_t ExecutionRegistry::count() const {
  return executions_.size();
}

void ExecutionRegistry::clear() {
  executions_.clear();
}

} // namespace openpuzzle
