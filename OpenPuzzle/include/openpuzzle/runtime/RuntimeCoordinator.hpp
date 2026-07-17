#pragma once

#include "openpuzzle/runtime/RuntimeTickResult.hpp"

namespace openpuzzle {

class Database;
class EngineManager;
class EventBus;
class SchedulingPolicy;
class WorkerAgentRegistry;

class RuntimeCoordinator {
public:
  RuntimeCoordinator(
      Database& database,
      WorkerAgentRegistry& workers,
      const SchedulingPolicy& schedulingPolicy,
      EngineManager& engineManager);

  RuntimeCoordinator(
      Database& database,
      WorkerAgentRegistry& workers,
      const SchedulingPolicy& schedulingPolicy,
      EngineManager& engineManager,
      EventBus& eventBus);

  RuntimeTickResult tick();

private:
  Database& database_;
  WorkerAgentRegistry& workers_;
  const SchedulingPolicy& schedulingPolicy_;
  EngineManager& engineManager_;
  EventBus* eventBus_ = nullptr;
};

} // namespace openpuzzle
