#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/runtime/SchedulerTick.hpp"
#include "openpuzzle/scheduler/SchedulingPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

class FixedSchedulingPolicy final : public SchedulingPolicy {
public:
  SchedulerDecision decide(Database&) const override {
    SchedulerDecision decision;
    decision.shouldDispatch = true;
    decision.jobId = 123;
    decision.workerId = 456;
    return decision;
  }
};

int main() {
  Database db;

  assert(db.open(":memory:"));
  assert(db.createSchema());

  FixedSchedulingPolicy policy;
  SchedulerTick tick(db, policy);

  auto decision = tick.execute();

  assert(decision.shouldDispatch);
  assert(decision.jobId == 123);
  assert(decision.workerId == 456);

  std::cout << "SchedulerPolicyInjectionTests passed\n";
  return 0;
}
