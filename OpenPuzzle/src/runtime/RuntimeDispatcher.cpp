#include "openpuzzle/runtime/RuntimeDispatcher.hpp"

#include "openpuzzle/database/Database.hpp"

namespace openpuzzle {

RuntimeDispatcher::RuntimeDispatcher(Database& database)
    : database_(database) {}

bool RuntimeDispatcher::dispatch(const SchedulerDecision& decision) const {
  if (!decision.shouldDispatch) {
    return false;
  }

  auto job = database_.getJob(decision.jobId);

  if (!job) {
    return false;
  }

  auto range = database_.getRange(job->rangeId);

  if (!range) {
    return false;
  }

  auto worker = database_.getWorker(decision.workerId);

  if (!worker) {
    return false;
  }

  return true;
}

} // namespace openpuzzle
