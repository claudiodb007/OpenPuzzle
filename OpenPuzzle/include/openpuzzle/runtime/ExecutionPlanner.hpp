#pragma once

#include "openpuzzle/runtime/ExecutionPlan.hpp"

#include <optional>

namespace openpuzzle {

class Database;
class WorkerAgentRegistry;
class CapabilityScheduler;

class ExecutionPlanner {
public:

  ExecutionPlanner(
      Database& database,
      WorkerAgentRegistry& workers);

  std::optional<ExecutionPlan>
  plan();

private:

  Database& database_;
  WorkerAgentRegistry& workers_;
};

}
