#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/runtime/RuntimeCoordinator.hpp"
#include "openpuzzle/scheduler/SchedulingPolicy.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

class IdleSchedulingPolicy final
    : public SchedulingPolicy {
public:
  SchedulerDecision decide(
      Database&) const override {
    return {};
  }
};

int main() {
  Database database;

  assert(database.open(":memory:"));
  assert(database.createSchema());

  WorkerRecord workerRecord;
  workerRecord.machine = "local-node";
  workerRecord.gpuName = "RTX 4070 Super";
  workerRecord.backend = "CUDA";
  workerRecord.engine = "BitCrack";
  workerRecord.status = "idle";

  const int workerId =
      database.upsertWorker(workerRecord);

  assert(workerId > 0);

  WorkerAgentInfo info;
  info.workerId = workerId;
  info.machine = workerRecord.machine;
  info.gpuName = workerRecord.gpuName;
  info.backend = workerRecord.backend;
  info.engine = workerRecord.engine;
  info.state = WorkerAgentState::Idle;

  WorkerAgentRegistry workers;
  assert(workers.add(WorkerAgent(info)));

  IdleSchedulingPolicy policy;

  EngineManager engineManager;

  RuntimeCoordinator coordinator(
      database,
      workers,
      policy,
      engineManager);

  auto result = coordinator.tick();

  assert(!result.decision.shouldDispatch);
  assert(!result.dispatched);
  assert(!result.launchSuccess);

  assert(result.monitor.running == 0);
  assert(result.monitor.finished == 0);
  assert(result.monitor.failed == 0);
  assert(result.monitor.cancelled == 0);

  auto updatedWorker =
      database.getWorker(workerId);

  assert(updatedWorker);
  assert(updatedWorker->status == "idle");

  std::cout
      << "RuntimeCoordinatorTests passed\n";

  return 0;
}
