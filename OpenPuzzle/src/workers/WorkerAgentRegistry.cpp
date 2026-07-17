#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <utility>

namespace openpuzzle {

bool WorkerAgentRegistry::add(WorkerAgent agent) {
  const int workerId = agent.info().workerId;

  if (workerId <= 0 || find(workerId)) {
    return false;
  }

  agents_.push_back(std::move(agent));
  return true;
}

WorkerAgent* WorkerAgentRegistry::find(int workerId) {
  for (auto& agent : agents_) {
    if (agent.info().workerId == workerId) {
      return &agent;
    }
  }

  return nullptr;
}

const WorkerAgent* WorkerAgentRegistry::find(int workerId) const {
  for (const auto& agent : agents_) {
    if (agent.info().workerId == workerId) {
      return &agent;
    }
  }

  return nullptr;
}

WorkerAgent* WorkerAgentRegistry::acquireIdle() {
  for (auto& agent : agents_) {
    if (agent.idle()) {
      return &agent;
    }
  }

  return nullptr;
}

WorkerAgent* WorkerAgentRegistry::acquireIdle(
    const std::string& engine,
    const std::string& backend) {
  WorkerAgent* selected = nullptr;
  double selectedSpeed = -1.0;

  for (auto& agent : agents_) {
    if (!agent.idle()) {
      continue;
    }

    const auto* capability =
        agent.bestCapability(engine, backend);

    double speed = 0.0;

    if (capability) {
      speed = capability->benchmarkSpeedMkeys;
    } else {
      if (!agent.info().capabilities.empty()) {
        continue;
      }

      if (!engine.empty() &&
          agent.info().engine != engine) {
        continue;
      }

      if (!backend.empty() &&
          agent.info().backend != backend) {
        continue;
      }

      speed = agent.info().speedMkeys;
    }

    if (!selected ||
        speed > selectedSpeed ||
        (speed == selectedSpeed &&
         agent.info().workerId <
             selected->info().workerId)) {
      selected = &agent;
      selectedSpeed = speed;
    }
  }

  return selected;
}

std::vector<WorkerAgent*> WorkerAgentRegistry::all() {
  std::vector<WorkerAgent*> result;
  result.reserve(agents_.size());

  for (auto& agent : agents_) {
    result.push_back(&agent);
  }

  return result;
}

std::vector<const WorkerAgent*> WorkerAgentRegistry::all() const {
  std::vector<const WorkerAgent*> result;
  result.reserve(agents_.size());

  for (const auto& agent : agents_) {
    result.push_back(&agent);
  }

  return result;
}

std::size_t WorkerAgentRegistry::count() const {
  return agents_.size();
}

void WorkerAgentRegistry::clear() {
  agents_.clear();
}

} // namespace openpuzzle
