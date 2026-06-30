#include "openpuzzle/core/ExecutionContext.hpp"

using namespace openpuzzle;

int main() {
  ExecutionContext ctx;

  ctx.executionId = 1;
  ctx.puzzleId = 71;
  ctx.jobId = 42;
  ctx.rangeId = 1001;
  ctx.engine = "BitCrack";
  ctx.workspace = "workspace";
  ctx.command = "command";
  ctx.echoOutput = false;

  if (ctx.executionId != 1)
    return 1;
  if (ctx.puzzleId != 71)
    return 2;
  if (ctx.jobId != 42)
    return 3;
  if (ctx.rangeId != 1001)
    return 4;
  if (ctx.engine != "BitCrack")
    return 5;
  if (ctx.workspace != "workspace")
    return 6;
  if (ctx.command != "command")
    return 7;
  if (ctx.echoOutput != false)
    return 8;

  return 0;
}
