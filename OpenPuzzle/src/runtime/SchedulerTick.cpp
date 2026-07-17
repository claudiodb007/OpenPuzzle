#include "openpuzzle/runtime/SchedulerTick.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/scheduler/DefaultSchedulingPolicy.hpp"
#include "openpuzzle/scheduler/SchedulingPolicy.hpp"

namespace openpuzzle {

SchedulerTick::SchedulerTick(Database& database)
    : database_(database) {}

SchedulerTick::SchedulerTick(
    Database& database,
    const SchedulingPolicy& policy)
    : database_(database),
      policy_(&policy) {}

SchedulerDecision SchedulerTick::execute() const {
  if (policy_) {
    return policy_->decide(database_);
  }

  DefaultSchedulingPolicy defaultPolicy;
  return defaultPolicy.decide(database_);
}

} // namespace openpuzzle
