#include "openpuzzle/core/Scheduler.hpp"

using namespace openpuzzle;

int main() {
  Scheduler scheduler;

  ExecutionContext ctx;
  ctx.jobId = 42;
  ctx.rangeId = 1001;

  ExecutionResult executionResult;
  executionResult.success = true;
  executionResult.exitCode = 0;

  auto result = scheduler.runOnce(ctx, executionResult);

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
