#include "openpuzzle/runtime/ExecutionLauncher.hpp"

#include "openpuzzle/core/ExecutionContext.hpp"

namespace openpuzzle {

ExecutionLauncher::ExecutionLauncher() = default;



ProcessContext ExecutionLauncher::buildProcessContext(
    const StartExecutionRequest& request) const {

  ProcessContext context;

  context.command = request.command;
  context.workspace = request.workspace;
  context.echoOutput = request.echoOutput;

  return context;
}

ExecutionResult ExecutionLauncher::launch(const StartExecutionRequest& request,
                                          int maxSeconds,
                                          int maxSamples) const {
  auto process = buildProcessContext(request);

  ExecutionContext context;

  context.executionId = request.executionId;
  context.puzzleId = request.puzzleId;
  context.jobId = request.jobId;
  context.rangeId = request.rangeId;
  context.engine = request.engine;

  context.command = process.command;
  context.workspace = process.workspace;
  context.echoOutput = process.echoOutput;
  context.onProgress = process.onProgress;

  return executionManager_.run(context, maxSeconds, maxSamples);
}

} // namespace openpuzzle
