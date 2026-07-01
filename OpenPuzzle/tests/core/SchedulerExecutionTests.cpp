#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/Scheduler.hpp"

using namespace openpuzzle;

int main() {
  Scheduler scheduler;
  ExecutionManager executionManager;

  ExecutionContext ctx;
  ctx.executionId = 1;
  ctx.puzzleId = 71;
  ctx.jobId = 42;
  ctx.rangeId = 1001;
  ctx.engine = "Test";
  ctx.command = "printf '[Info] 1600.00 MKey/s\\nFinished\\n'";
  ctx.echoOutput = false;

  auto result = scheduler.runExecution(ctx, executionManager);

  if (!result.success)
    return 1;
  if (result.jobId != 42)
    return 2;
  if (result.rangeId != 1001)
    return 3;
  if (result.exitCode != 0)
    return 4;

  return 0;
}
