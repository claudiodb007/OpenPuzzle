#pragma once

#include "openpuzzle/runtime/SchedulerDecision.hpp"

namespace openpuzzle {

class Database;

class SchedulingPolicy {
public:
  virtual ~SchedulingPolicy() = default;

  virtual SchedulerDecision decide(Database& database) = 0;
};

} // namespace openpuzzle
