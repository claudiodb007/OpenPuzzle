#include "openpuzzle/runtime/RuntimeTickResult.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

int main() {
  RuntimeTickResult result;

  ExecutionPlan plan;
  plan.workerId = 7;
  plan.jobId = 12;
  plan.engine = "BitCrack";
  plan.backend = "CUDA";
  plan.expectedSpeedMkeys = 1550.0;
  plan.valid = true;

  result.plan = plan;

  result.decision.shouldDispatch =
      result.plan->valid;

  result.decision.jobId =
      result.plan->jobId;

  result.decision.workerId =
      result.plan->workerId;

  assert(result.plan);
  assert(result.plan->workerId == 7);
  assert(result.plan->jobId == 12);
  assert(result.plan->engine == "BitCrack");
  assert(result.plan->backend == "CUDA");

  assert(result.decision.shouldDispatch);
  assert(result.decision.jobId == 12);
  assert(result.decision.workerId == 7);

  std::cout
      << "RuntimeTickResultTests passed\n";

  return 0;
}
