#pragma once

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"

namespace openpuzzle {

class RuntimeManager {
public:
  RuntimeManager();

  ExecutionResult run(const ExecutionContext& context,
                      int maxSeconds = 0,
                      int maxSamples = 0) const;

private:
  ExecutionManager executionManager_;
};

} // namespace openpuzzle
