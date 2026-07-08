#pragma once

#include "openpuzzle/runtime/ExecutionState.hpp"

#include <optional>
#include <vector>

namespace openpuzzle {

class ExecutionRegistry {
public:
  void add(const ExecutionState& state);

  std::optional<ExecutionState> find(int executionId) const;

  std::vector<ExecutionState> all() const;
  std::vector<ExecutionState> active() const;
  std::vector<ExecutionState> finished() const;
  std::vector<ExecutionState> failed() const;

  bool update(const ExecutionState& state);

  std::size_t count() const;
  void clear();

private:
  std::vector<ExecutionState> executions_;
};

} // namespace openpuzzle
