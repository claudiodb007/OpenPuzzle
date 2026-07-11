#pragma once

#include "openpuzzle/runtime/ExecutionResource.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"

#include <vector>

namespace openpuzzle {

class WorkerAgentFactory {
public:
  static WorkerAgent create(
      const ExecutionResource& resource);

  static WorkerAgent create(
      const ExecutionResource& resource,
      WorkerAgentInfo info);

  static std::vector<WorkerAgent> create(
      const std::vector<ExecutionResource>& resources);
};

} // namespace openpuzzle
