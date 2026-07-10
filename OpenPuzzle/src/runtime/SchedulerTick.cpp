#include "openpuzzle/runtime/SchedulerTick.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/dispatcher/WorkerSelector.hpp"

namespace openpuzzle {

SchedulerTick::SchedulerTick(Database& database)
    : database_(database) {}

SchedulerDecision SchedulerTick::execute() const {
  SchedulerDecision decision;

  auto job = database_.nextReservedJob();

  if (!job) {
    return decision;
  }

  WorkerSelector selector(database_);
  auto worker = selector.selectIdleWorker();

  if (!worker) {
    return decision;
  }

  decision.shouldDispatch = true;
  decision.jobId = job->id;
  decision.workerId = worker->id;

  return decision;
}

} // namespace openpuzzle
