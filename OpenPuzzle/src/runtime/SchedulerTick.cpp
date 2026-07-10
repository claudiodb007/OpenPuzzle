#include "openpuzzle/runtime/SchedulerTick.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/scheduler/DefaultSchedulingPolicy.hpp"

namespace openpuzzle {

SchedulerTick::SchedulerTick(Database& database)
    : database_(database) {}

SchedulerDecision SchedulerTick::execute() const {
  DefaultSchedulingPolicy policy;
  return policy.decide(database_);
}

} // namespace openpuzzle
