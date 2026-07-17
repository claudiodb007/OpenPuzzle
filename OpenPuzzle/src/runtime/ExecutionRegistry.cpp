#include "openpuzzle/runtime/ExecutionRegistry.hpp"

namespace openpuzzle {

void ExecutionRegistry::add(const ExecutionState& state) {
  executions_.emplace_back(state);
}

std::optional<ExecutionState> ExecutionRegistry::find(int executionId) const {
  for (const auto& execution : executions_) {
    if (execution.state().executionId == executionId) {
      return execution.state();
    }
  }

  return std::nullopt;
}

std::vector<ExecutionState> ExecutionRegistry::all() const {
  std::vector<ExecutionState> result;

  for (const auto& execution : executions_) {
    result.push_back(execution.state());
  }

  return result;
}

std::vector<ExecutionState> ExecutionRegistry::active() const {
  std::vector<ExecutionState> result;

  for (const auto& execution : executions_) {
    if (execution.isPending() || execution.isRunning()) {
      result.push_back(execution.state());
    }
  }

  return result;
}

std::vector<ExecutionState> ExecutionRegistry::finished() const {
  std::vector<ExecutionState> result;

  for (const auto& execution : executions_) {
    if (execution.isFinished()) {
      result.push_back(execution.state());
    }
  }

  return result;
}

std::vector<ExecutionState> ExecutionRegistry::failed() const {
  std::vector<ExecutionState> result;

  for (const auto& execution : executions_) {
    if (execution.isFailed()) {
      result.push_back(execution.state());
    }
  }

  return result;
}

bool ExecutionRegistry::update(const ExecutionState& state) {
  for (auto& execution : executions_) {
    if (execution.state().executionId == state.executionId) {
      execution.state() = state;
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
