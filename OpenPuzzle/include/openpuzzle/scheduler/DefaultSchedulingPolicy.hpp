#pragma once

#include "openpuzzle/scheduler/SchedulingPolicy.hpp"

namespace openpuzzle {

class DefaultSchedulingPolicy : public SchedulingPolicy {
public:
  SchedulerDecision decide(Database& database) override;
};

} // namespace openpuzzle
