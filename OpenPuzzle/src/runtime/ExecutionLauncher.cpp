#include "openpuzzle/runtime/ExecutionLauncher.hpp"

#include "openpuzzle/core/ExecutionContext.hpp"

namespace openpuzzle {

ExecutionLauncher::ExecutionLauncher() = default;

ExecutionResult ExecutionLauncher::launch(const StartExecutionRequest& request,
                                          int maxSeconds,
                                          int maxSamples) const {
  ExecutionContext context;

  context.executionId = request.executionId;
  context.puzzleId = request.puzzleId;
  context.jobId = request.jobId;
  context.rangeId = request.rangeId;
  context.engine = request.engine;
  context.workspace = request.workspace;
  context.command = request.command;
  context.echoOutput = request.echoOutput;

  return executionManager_.run(context, maxSeconds, maxSamples);
}

} // namespace openpuzzle
