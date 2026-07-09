#include "openpuzzle/runtime/SchedulerTick.hpp"

#include "openpuzzle/database/Database.hpp"

namespace openpuzzle {

SchedulerTick::SchedulerTick(Database& database)
    : database_(database) {}

SchedulerDecision SchedulerTick::execute() const {
  SchedulerDecision decision;

  auto job = database_.nextReservedJob();

  if (!job) {
    return decision;
  }

  auto workers = database_.listWorkers();

  if (workers.empty()) {
    return decision;
  }

  decision.shouldDispatch = true;
  decision.jobId = job->id;
  decision.workerId = workers.front().id;

  return decision;
}

} // namespace openpuzzle
