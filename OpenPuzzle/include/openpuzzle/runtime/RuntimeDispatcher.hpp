#pragma once

#include "openpuzzle/runtime/SchedulerDecision.hpp"

namespace openpuzzle {

class Database;

class RuntimeDispatcher {
public:
  explicit RuntimeDispatcher(Database& database);

  bool dispatch(const SchedulerDecision& decision) const;

private:
  Database& database_;
};

} // namespace openpuzzle
