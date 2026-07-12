#include "openpuzzle/runtime/RuntimeCoordinator.hpp"

#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/runtime/ExecutionPlanner.hpp"
#include "openpuzzle/runtime/RuntimeDispatcher.hpp"
#include "openpuzzle/scheduler/SchedulingPolicy.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"
#include "openpuzzle/workers/WorkerLifecycle.hpp"

namespace openpuzzle {

RuntimeCoordinator::RuntimeCoordinator(
    Database& database,
    WorkerAgentRegistry& workers,
    const SchedulingPolicy& schedulingPolicy,
    EngineManager& engineManager)
    : database_(database),
      workers_(workers),
      schedulingPolicy_(schedulingPolicy),
      engineManager_(engineManager) {}

RuntimeCoordinator::RuntimeCoordinator(
    Database& database,
    WorkerAgentRegistry& workers,
    const SchedulingPolicy& schedulingPolicy,
    EngineManager& engineManager,
    EventBus& eventBus)
    : database_(database),
      workers_(workers),
      schedulingPolicy_(schedulingPolicy),
      engineManager_(engineManager),
      eventBus_(&eventBus) {}

RuntimeTickResult RuntimeCoordinator::tick() {
  RuntimeTickResult result;

  WorkerLifecycle lifecycle(
      database_,
      workers_);
lifecycle.refreshHeartbeats();

  ExecutionPlanner planner(
      database_,
      workers_);

  result.plan = planner.plan();

  if (result.plan) {
    result.decision.shouldDispatch =
        result.plan->valid;

    result.decision.jobId =
        result.plan->jobId;

    result.decision.workerId =
        result.plan->workerId;
  }

  RuntimeDispatcher dispatcher(
      database_,
      workers_,
      engineManager_);

  if (result.plan &&
      dispatcher.dispatch(*result.plan)) {
    auto executionResult =
        dispatcher.dispatchAndLaunch(
            *result.plan);

    result.dispatched = true;

    result.launchSuccess =
        executionResult.success;

    result.exitCode =
        executionResult.exitCode;
}

  result.monitor =
      lifecycle.monitorExecutions();

  return result;
}

} // namespace openpuzzle
