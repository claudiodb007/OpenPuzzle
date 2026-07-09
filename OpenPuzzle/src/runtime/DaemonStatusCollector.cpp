#include "openpuzzle/runtime/DaemonStatusCollector.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/models/Models.hpp"

namespace openpuzzle {

DaemonStatusCollector::DaemonStatusCollector(Database& database)
    : database_(database) {}

DaemonStatus DaemonStatusCollector::collect() const {
  DaemonStatus status;

  status.reservedJobs =
      static_cast<int>(database_.countJobsByState(0, JobState::Reserved));

  status.runningJobs =
      static_cast<int>(database_.countJobsByState(0, JobState::Running));

  status.runningExecutions = 0;

  for (const auto& execution : database_.listExecutions()) {
    if (ExecutionRecord::statusToString(execution.status) == "RUNNING") {
      ++status.runningExecutions;
    }
  }

  status.workers = static_cast<int>(database_.listWorkers().size());

  return status;
}

} // namespace openpuzzle
