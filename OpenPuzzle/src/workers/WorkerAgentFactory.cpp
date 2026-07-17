#include "openpuzzle/workers/WorkerAgentFactory.hpp"

#include <utility>

namespace openpuzzle {

WorkerAgent WorkerAgentFactory::create(
    const ExecutionResource& resource) {
  WorkerAgentInfo info;

  info.machine = resource.name;
  info.gpuName = resource.name;
  info.engine = resource.engine;
  info.backend = resource.backend;
  info.state = WorkerAgentState::Idle;

  return create(resource, std::move(info));
}

WorkerAgent WorkerAgentFactory::create(
    const ExecutionResource& resource,
    WorkerAgentInfo info) {
  if (info.machine.empty()) {
    info.machine = resource.name;
  }

  if (info.gpuName.empty()) {
    info.gpuName = resource.name;
  }

  if (info.engine.empty()) {
    info.engine = resource.engine;
  }

  if (info.backend.empty()) {
    info.backend = resource.backend;
  }

  info.capabilities.push_back(
      resource.capability);

  return WorkerAgent(std::move(info));
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
