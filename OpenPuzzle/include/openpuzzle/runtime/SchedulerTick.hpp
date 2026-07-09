#pragma once

#include "openpuzzle/runtime/SchedulerDecision.hpp"

namespace openpuzzle {

class Database;

class SchedulerTick {
public:
  explicit SchedulerTick(Database& database);

  SchedulerDecision execute() const;

private:
  Database& database_;
};

} // namespace openpuzzle
