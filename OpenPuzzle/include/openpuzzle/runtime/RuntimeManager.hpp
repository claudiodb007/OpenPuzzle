#pragma once

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/runtime/ExecutionState.hpp"

#include <vector>

namespace openpuzzle {

class RuntimeManager {
public:
  RuntimeManager();

  ExecutionResult run(const ExecutionContext& context,
                      int maxSeconds = 0,
                      int maxSamples = 0);

  const std::vector<ExecutionState>& activeExecutions() const;

private:
  ExecutionManager executionManager_;
  std::vector<ExecutionState> activeExecutions_;
};

} // namespace openpuzzle
