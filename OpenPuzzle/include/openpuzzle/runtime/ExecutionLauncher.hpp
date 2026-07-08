#pragma once

#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/engines/EngineOutputParser.hpp"
#include "openpuzzle/runtime/Execution.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/runtime/ProcessContext.hpp"

#include <memory>

namespace openpuzzle {

class ExecutionLauncher {
public:
  ExecutionLauncher();

  ExecutionResult launch(const StartExecutionRequest& request,
                         int maxSeconds = 0,
                         int maxSamples = 0) const;

  ExecutionResult launch(Execution& execution,
                         const StartExecutionRequest& request,
                         std::unique_ptr<EngineOutputParser> parser,
                         int maxSeconds = 0,
                         int maxSamples = 0) const;

private:
  ProcessContext buildProcessContext(const StartExecutionRequest& request) const;

  ExecutionManager executionManager_;
};

} // namespace openpuzzle
