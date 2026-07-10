#pragma once

#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/runtime/SchedulerDecision.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

namespace openpuzzle {

class Database;
class WorkerAgentRegistry;

class RuntimeDispatcher {
public:
  RuntimeDispatcher(Database& database,
                    WorkerAgentRegistry& workers);

  bool dispatch(const SchedulerDecision& decision) const;
  StartExecutionRequest prepare(const SchedulerDecision& decision) const;
  ExecutionResult dispatchAndLaunch(const SchedulerDecision& decision) const;

private:
  Database& database_;
  WorkerAgentRegistry& workers_;
};

} // namespace openpuzzle
