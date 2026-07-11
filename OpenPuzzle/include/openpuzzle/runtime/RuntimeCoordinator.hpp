#pragma once

#include "openpuzzle/runtime/RuntimeTickResult.hpp"

namespace openpuzzle {

class Database;
class SchedulingPolicy;
class WorkerAgentRegistry;

class RuntimeCoordinator {
public:
  RuntimeCoordinator(
      Database& database,
      WorkerAgentRegistry& workers,
      const SchedulingPolicy& schedulingPolicy);

  RuntimeTickResult tick();

private:
  Database& database_;
  WorkerAgentRegistry& workers_;
  const SchedulingPolicy& schedulingPolicy_;
};

} // namespace openpuzzle
