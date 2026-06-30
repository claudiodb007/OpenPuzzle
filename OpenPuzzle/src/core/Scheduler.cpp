#include "openpuzzle/core/Scheduler.hpp"

namespace openpuzzle {

SchedulerResult
Scheduler::runOnce(const ExecutionContext &context,
                   const ExecutionResult &executionResult) const {
  SchedulerResult result;

  result.success = executionResult.success;
  result.jobId = context.jobId;
  result.rangeId = context.rangeId;
  result.exitCode = executionResult.exitCode;

  return result;
}

SchedulerResult
Scheduler::runOnceWithEvents(const ExecutionContext &context,
                             const ExecutionResult &executionResult,
                             EventBus &bus) const {
  bus.publish(Event{EventType::ExecutionStarted, context.executionId,
                    context.jobId, "Scheduler cycle started", "", 0.0});

  auto result = runOnce(context, executionResult);

  bus.publish(Event{
      result.success ? EventType::ExecutionFinished : EventType::Error,
      context.executionId, context.jobId,
      result.success ? "Scheduler cycle finished" : "Scheduler cycle failed",
      "", static_cast<double>(result.exitCode)});

  return result;
}

} // namespace openpuzzle
