#include "openpuzzle/workers/WorkerAgentFactory.hpp"

namespace openpuzzle {

WorkerAgent WorkerAgentFactory::create(
    const ExecutionResource& resource) {
  WorkerAgentInfo info;

  info.machine = resource.name;
  info.engine = resource.engine;
  info.backend = resource.backend;
  info.state = WorkerAgentState::Idle;

  info.capabilities.push_back(
      resource.capability);

  return WorkerAgent(info);
}

std::vector<WorkerAgent>
WorkerAgentFactory::create(
    const std::vector<ExecutionResource>& resources) {
  std::vector<WorkerAgent> workers;
  workers.reserve(resources.size());

  for (const auto& resource : resources) {
    workers.push_back(create(resource));
  }

  return workers;
}

} // namespace openpuzzle
