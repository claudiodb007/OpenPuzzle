#include "openpuzzle/runtime/RuntimeCoordinator.hpp"

#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/runtime/RuntimeDispatcher.hpp"
#include "openpuzzle/runtime/SchedulerTick.hpp"
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

  WorkerLifecycle lifecycle =
      eventBus_
          ? WorkerLifecycle(
                database_,
                workers_,
                *eventBus_)
          : WorkerLifecycle(
                database_,
                workers_);

  lifecycle.refreshHeartbeats();

  SchedulerTick scheduler(
      database_,
      schedulingPolicy_);

  result.decision = scheduler.execute();

  RuntimeDispatcher dispatcher(
      database_,
      workers_,
      engineManager_);

  if (dispatcher.dispatch(result.decision)) {
    auto executionResult =
        dispatcher.dispatchAndLaunch(
            result.decision);

    result.dispatched = true;
    result.launchSuccess =
        executionResult.success;
    result.exitCode =
        executionResult.exitCode;

    if (eventBus_) {
      Event event;
      event.type =
          EventType::ExecutionDispatched;
      event.jobId =
          result.decision.jobId;
      event.workerId =
          result.decision.workerId;
      event.exitCode =
          result.exitCode;
      event.message =
          "Job dispatched to worker";

      eventBus_->publish(event);
    }
  }

  result.monitor =
      lifecycle.monitorExecutions();

  return result;
}

} // namespace openpuzzle
