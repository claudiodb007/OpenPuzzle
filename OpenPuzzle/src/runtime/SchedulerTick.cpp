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

  for (const auto& worker : workers) {
    if (worker.status != "idle") {
      continue;
    }

    decision.shouldDispatch = true;
    decision.jobId = job->id;
    decision.workerId = worker.id;
    return decision;
  }

  return decision;
}

} // namespace openpuzzle
