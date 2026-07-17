#include "openpuzzle/scheduler/CapabilityScheduler.hpp"

#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

namespace openpuzzle {

CapabilityScheduler::CapabilityScheduler(
    WorkerAgentRegistry& workers)
    : workers_(workers) {}

std::optional<CapabilitySelection>
CapabilityScheduler::select(
    const std::string& engine,
    const std::string& backend) const {
  const WorkerAgent* selectedWorker = nullptr;
  const WorkerEngineCapability* selectedCapability = nullptr;
  double selectedSpeed = -1.0;

  for (const auto* worker : workers_.all()) {
    if (!worker || !worker->idle()) {
      continue;
    }

    const auto* capability =
        worker->bestCapability(
            engine,
            backend);

    if (!capability) {
      continue;
    }

    const double speed =
        capability->benchmarkSpeedMkeys;

    if (!selectedWorker ||
        speed > selectedSpeed ||
        (speed == selectedSpeed &&
         worker->info().workerId <
             selectedWorker->info().workerId)) {
      selectedWorker = worker;
      selectedCapability = capability;
      selectedSpeed = speed;
    }
  }

  if (!selectedWorker ||
      !selectedCapability) {
    return std::nullopt;
  }

  CapabilitySelection selection;

  selection.workerId =
      selectedWorker->info().workerId;

  selection.engine =
      selectedCapability->engine;

  selection.backend =
      selectedCapability->backend;

  selection.capability =
      *selectedCapability;

  selection.expectedSpeedMkeys =
      selectedCapability->benchmarkSpeedMkeys;

  selection.valid = true;

  return selection;
}

} // namespace openpuzzle
