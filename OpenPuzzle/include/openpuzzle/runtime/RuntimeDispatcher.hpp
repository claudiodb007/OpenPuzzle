#pragma once

#include "openpuzzle/runtime/SchedulerDecision.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"

namespace openpuzzle {

class Database;

class RuntimeDispatcher {
public:
  explicit RuntimeDispatcher(Database& database);

  bool dispatch(const SchedulerDecision& decision) const;
  StartExecutionRequest prepare(const SchedulerDecision& decision) const;
  ExecutionResult dispatchAndLaunch(const SchedulerDecision& decision) const;

private:
  Database& database_;
};

} // namespace openpuzzle
