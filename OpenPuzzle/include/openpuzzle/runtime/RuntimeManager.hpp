#pragma once

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/runtime/ExecutionRegistry.hpp"
#include "openpuzzle/runtime/ExecutionState.hpp"

#include <vector>

namespace openpuzzle {

class RuntimeManager {
public:
  RuntimeManager();

  ExecutionResult run(const ExecutionContext& context,
                      int maxSeconds = 0,
                      int maxSamples = 0);

  std::vector<ExecutionState> activeExecutions() const;
  const ExecutionRegistry& registry() const;

private:
  ExecutionManager executionManager_;
  ExecutionRegistry registry_;
};

} // namespace openpuzzle
