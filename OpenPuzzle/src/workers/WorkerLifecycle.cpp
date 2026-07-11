#include "openpuzzle/workers/WorkerLifecycle.hpp"

#include "openpuzzle/core/ExecutionRecord.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/services/HeartbeatService.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

namespace openpuzzle {

WorkerLifecycle::WorkerLifecycle(
    Database& database,
    WorkerAgentRegistry& workers)
    : database_(database),
      workers_(workers) {}

void WorkerLifecycle::refreshHeartbeats(
    int timeoutSeconds) {
  HeartbeatService heartbeat(database_);

  for (const auto* worker : workers_.all()) {
    if (!worker || worker->offline()) {
      continue;
    }

    heartbeat.update(
        worker->info().workerId,
        WorkerAgent::stateToString(worker->state()),
        worker->info().speedMkeys,
        worker->info().temperature,
        worker->info().power);
  }

  heartbeat.expireStale(timeoutSeconds);
}

ExecutionProcessMonitorSummary
WorkerLifecycle::monitorExecutions() {
  ExecutionProcessMonitor monitor(database_);

  auto summary = monitor.poll();

  synchronizeCompletedExecutions();

  return summary;
}

void WorkerLifecycle::synchronizeCompletedExecutions() {
  for (auto* worker : workers_.all()) {
    if (!worker || !worker->hasExecution()) {
      continue;
    }

    const auto& handle =
        worker->currentExecution();

    if (!handle) {
      continue;
    }

    auto execution =
        database_.getExecution(
            handle->executionId);

    if (!execution) {
      continue;
    }

    if (execution->status ==
        ExecutionRecordStatus::Running) {
      continue;
    }

    worker->completeExecution();

    database_.updateWorkerStatus(
        worker->info().workerId,
        WorkerAgent::stateToString(
            worker->state()));
  }
}

} // namespace openpuzzle
