#include "openpuzzle/core/Scheduler.hpp"

namespace openpuzzle {

SchedulerResult Scheduler::runOnce(const ExecutionContext& context, const ExecutionResult& executionResult) const {
    SchedulerResult result;

    result.success = executionResult.success;
    result.jobId = context.jobId;
    result.rangeId = context.rangeId;
    result.exitCode = executionResult.exitCode;

    return result;
}

} // namespace openpuzzle
