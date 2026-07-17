#include "openpuzzle/dispatcher/WorkerSelector.hpp"

#include "openpuzzle/database/Database.hpp"

namespace openpuzzle {

WorkerSelector::WorkerSelector(Database& database)
    : database_(database) {}

bool WorkerSelector::betterCandidate(
    const WorkerRecord& candidate,
    const WorkerRecord& current) {
  if (candidate.speedMkeys != current.speedMkeys) {
    return candidate.speedMkeys > current.speedMkeys;
  }

  return candidate.id < current.id;
}

std::optional<WorkerRecord>
WorkerSelector::selectIdleWorker() {
  return selectIdleWorker("", "");
}

std::optional<WorkerRecord>
WorkerSelector::selectIdleWorker(
    const std::string& engine,
    const std::string& backend) {
  std::optional<WorkerRecord> selected;

  for (const auto& worker : database_.listWorkers()) {
    if (worker.status != "idle") {
      continue;
    }

    if (!engine.empty() && worker.engine != engine) {
      continue;
    }

    if (!backend.empty() && worker.backend != backend) {
      continue;
    }

    if (!selected || betterCandidate(worker, *selected)) {
      selected = worker;
    }
  }

  return selected;
}

} // namespace openpuzzle
