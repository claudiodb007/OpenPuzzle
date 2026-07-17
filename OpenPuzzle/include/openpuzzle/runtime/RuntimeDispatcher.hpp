#pragma once

#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/runtime/ExecutionPlan.hpp"
#include "openpuzzle/runtime/SchedulerDecision.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

namespace openpuzzle {

class Database;
class EngineManager;
class WorkerAgentRegistry;

class RuntimeDispatcher {
public:
  RuntimeDispatcher(
      Database& database,
      WorkerAgentRegistry& workers,
      EngineManager& engineManager);

  bool dispatch(
      const ExecutionPlan& plan) const;

  StartExecutionRequest prepare(
      const ExecutionPlan& plan) const;

  ExecutionResult dispatchAndLaunch(
      const ExecutionPlan& plan) const;

  // Compatibilidade temporária.
  bool dispatch(
      const SchedulerDecision& decision) const;

  StartExecutionRequest prepare(
      const SchedulerDecision& decision) const;

  ExecutionResult dispatchAndLaunch(
      const SchedulerDecision& decision) const;

private:
  ExecutionPlan planFromDecision(
      const SchedulerDecision& decision) const;

  Database& database_;
  WorkerAgentRegistry& workers_;
  EngineManager& engineManager_;
};

} // namespace openpuzzle
