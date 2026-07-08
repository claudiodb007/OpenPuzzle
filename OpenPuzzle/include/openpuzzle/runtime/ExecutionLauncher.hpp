#pragma once

#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

namespace openpuzzle {

class ExecutionLauncher {
public:
  ExecutionLauncher();

  ExecutionResult launch(const StartExecutionRequest& request,
                         int maxSeconds = 0,
                         int maxSamples = 0) const;

private:
  ExecutionManager executionManager_;
};

} // namespace openpuzzle
