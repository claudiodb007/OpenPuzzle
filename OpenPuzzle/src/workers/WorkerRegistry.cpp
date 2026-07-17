#include "openpuzzle/workers/WorkerRegistry.hpp"

namespace openpuzzle {

void WorkerRegistry::add(const WorkerState& state) {
  workers_.emplace_back(state);
}

std::optional<WorkerState> WorkerRegistry::find(int workerId) const {
  for (const auto& worker : workers_) {
    if (worker.state().workerId == workerId) {
      return worker.state();
    }
  }

  return std::nullopt;
}

std::vector<WorkerState> WorkerRegistry::all() const {
  std::vector<WorkerState> result;

  for (const auto& worker : workers_) {
    result.push_back(worker.state());
  }

  return result;
}

std::vector<WorkerState> WorkerRegistry::online() const {
  std::vector<WorkerState> result;

  for (const auto& worker : workers_) {
    if (worker.isOnline()) {
      result.push_back(worker.state());
    }
  }

  return result;
}

std::vector<WorkerState> WorkerRegistry::busy() const {
  std::vector<WorkerState> result;

  for (const auto& worker : workers_) {
    if (worker.isBusy()) {
      result.push_back(worker.state());
    }
  }

  return result;
}

std::vector<WorkerState> WorkerRegistry::offline() const {
  std::vector<WorkerState> result;

  for (const auto& worker : workers_) {
    if (worker.isOffline()) {
      result.push_back(worker.state());
    }
  }

  return result;
}

bool WorkerRegistry::update(const WorkerState& state) {
  for (auto& worker : workers_) {
    if (worker.state().workerId == state.workerId) {
      worker.state() = state;
      return true;
    }
  }

  return false;
}

std::size_t WorkerRegistry::count() const {
  return workers_.size();
}

void WorkerRegistry::clear() {
  workers_.clear();
}

} // namespace openpuzzle
