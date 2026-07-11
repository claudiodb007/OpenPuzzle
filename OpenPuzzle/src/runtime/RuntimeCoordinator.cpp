#include "openpuzzle/runtime/RuntimeCoordinator.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/runtime/RuntimeDispatcher.hpp"
#include "openpuzzle/runtime/SchedulerTick.hpp"
#include "openpuzzle/scheduler/SchedulingPolicy.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"
#include "openpuzzle/workers/WorkerLifecycle.hpp"

namespace openpuzzle {

RuntimeCoordinator::RuntimeCoordinator(
    Database& database,
    WorkerAgentRegistry& workers,
    const SchedulingPolicy& schedulingPolicy)
    : database_(database),
      workers_(workers),
      schedulingPolicy_(schedulingPolicy) {}

RuntimeTickResult RuntimeCoordinator::tick() {
  RuntimeTickResult result;

  WorkerLifecycle lifecycle(
      database_,
      workers_);

  lifecycle.refreshHeartbeats();

  SchedulerTick scheduler(
      database_,
      schedulingPolicy_);

  result.decision = scheduler.execute();

  RuntimeDispatcher dispatcher(
      database_,
      workers_);

  if (dispatcher.dispatch(result.decision)) {
    auto executionResult =
        dispatcher.dispatchAndLaunch(
            result.decision);

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
