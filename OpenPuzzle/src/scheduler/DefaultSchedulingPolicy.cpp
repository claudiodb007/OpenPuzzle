#include "openpuzzle/scheduler/DefaultSchedulingPolicy.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/dispatcher/WorkerSelector.hpp"

namespace openpuzzle {

SchedulerDecision
DefaultSchedulingPolicy::decide(Database& database) {
  SchedulerDecision decision;

  auto job = database.nextReservedJob();

  if (!job) {
    return decision;
  }

  WorkerSelector selector(database);
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
