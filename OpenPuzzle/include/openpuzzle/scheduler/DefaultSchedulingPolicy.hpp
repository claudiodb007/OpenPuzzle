#pragma once

#include "openpuzzle/scheduler/SchedulingPolicy.hpp"

namespace openpuzzle {

class DefaultSchedulingPolicy : public SchedulingPolicy {
public:
  SchedulerDecision decide(Database& database) const override;
};

} // namespace openpuzzle
