#include "openpuzzle/runtime/ExecutionLauncher.hpp"

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/runtime/ExecutionMonitor.hpp"

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


ExecutionResult ExecutionLauncher::launch(
    Execution& execution,
    const StartExecutionRequest& request,
    std::unique_ptr<EngineOutputParser> parser,
    int maxSeconds,
    int maxSamples) const {

  auto process = buildProcessContext(request);
  ExecutionMonitor monitor(std::move(parser));

  ExecutionContext context;

  context.executionId = request.executionId;
  context.puzzleId = request.puzzleId;
  context.jobId = request.jobId;
  context.rangeId = request.rangeId;
  context.engine = request.engine;

  context.command = process.command;
  context.workspace = process.workspace;
  context.echoOutput = process.echoOutput;

  context.onProgress = [&](const ExecutionResult& progress) {
    if (!progress.keysChecked.empty() || progress.averageSpeed > 0.0 ||
        progress.keyFound) {
      ExecutionProgress executionProgress;
      executionProgress.speedMKeys = progress.averageSpeed;
      executionProgress.keysChecked = progress.keysChecked;
      executionProgress.keyFound = progress.keyFound;
      executionProgress.privateKey = progress.privateKey;
      execution.updateProgress(executionProgress);
    }

    if (process.onProgress) {
      process.onProgress(progress);
    }
  };

  auto result = executionManager_.run(context, maxSeconds, maxSamples);

  if (result.success) {
    execution.finish(result);
  } else {
    execution.fail(result);
  }

  return result;
}

} // namespace openpuzzle
