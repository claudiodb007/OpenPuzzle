#pragma once

#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/scheduler/DefaultSchedulingPolicy.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

namespace openpuzzle {

class Database;

class DaemonRunner {
public:
  explicit DaemonRunner(Database& database);

  int run(int ticks = 3);
  void stop();

private:
  void loadWorkers();
  void tick();

  Database& database_;
  WorkerAgentRegistry workers_;
  DefaultSchedulingPolicy schedulingPolicy_;
  EngineManager engineManager_;

  bool running_ = false;
  int tickCount_ = 0;
};

} // namespace openpuzzle
