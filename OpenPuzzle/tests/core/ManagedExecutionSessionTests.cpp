#include "openpuzzle/core/ManagedExecutionSession.hpp"

using namespace openpuzzle;

int main() {
  ExecutionContext ctx;
  ctx.executionId = 7;
  ctx.jobId = 42;
  ctx.command = "printf test";

  ManagedExecutionSession session(ctx);

  if (session.context().executionId != 7)
    return 1;
  if (session.context().jobId != 42)
    return 2;
  if (session.context().command != "printf test")
    return 3;
  if (session.running())
    return 4;
  if (session.handle().pid != -1)
    return 5;

  return 0;
}
