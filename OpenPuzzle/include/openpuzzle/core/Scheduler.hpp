#pragma once

#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"

namespace openpuzzle {

struct SchedulerResult {
  bool success = false;
  int jobId = 0;
  int rangeId = 0;
  int exitCode = -1;
};

class Scheduler {
public:
  SchedulerResult runOnce(const ExecutionContext &context,
                          const ExecutionResult &executionResult) const;
  SchedulerResult runOnceWithEvents(const ExecutionContext &context,
                                    const ExecutionResult &executionResult,
                                    EventBus &bus) const;

  SchedulerResult runExecution(const ExecutionContext &context,
                               const ExecutionManager &executionManager) const;
};

} // namespace openpuzzle
