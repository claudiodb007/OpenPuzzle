#pragma once

namespace openpuzzle {

struct SchedulerDecision {
  bool shouldDispatch = false;

  int jobId = 0;
  int workerId = 0;
  int executionId = 0;
};

} // namespace openpuzzle
