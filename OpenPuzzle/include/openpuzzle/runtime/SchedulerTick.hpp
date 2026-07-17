#pragma once

#include "openpuzzle/runtime/SchedulerDecision.hpp"

namespace openpuzzle {

class Database;
class SchedulingPolicy;

class SchedulerTick {
public:
  explicit SchedulerTick(Database& database);

  SchedulerTick(Database& database,
                const SchedulingPolicy& policy);

  SchedulerDecision execute() const;

private:
  Database& database_;
  const SchedulingPolicy* policy_ = nullptr;
};

} // namespace openpuzzle
