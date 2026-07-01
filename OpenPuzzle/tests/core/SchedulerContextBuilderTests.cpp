#include "openpuzzle/core/Scheduler.hpp"

using namespace openpuzzle;

int main() {
  Scheduler scheduler;

  auto ctx = scheduler.buildExecutionContext(7, 71, 42, 1001, "BitCrack",
                                             "/tmp/openpuzzle/jobs/00000042",
                                             "printf test", false);

  if (ctx.executionId != 7)
    return 1;
  if (ctx.puzzleId != 71)
    return 2;
  if (ctx.jobId != 42)
    return 3;
  if (ctx.rangeId != 1001)
    return 4;
  if (ctx.engine != "BitCrack")
    return 5;
  if (ctx.workspace != "/tmp/openpuzzle/jobs/00000042")
    return 6;
  if (ctx.command != "printf test")
    return 7;
  if (ctx.echoOutput != false)
    return 8;

  return 0;
}
