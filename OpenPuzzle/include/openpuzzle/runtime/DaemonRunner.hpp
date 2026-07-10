#pragma once

#include "openpuzzle/workers/WorkerAgentRegistry.hpp"
#include "openpuzzle/scheduler/DefaultSchedulingPolicy.hpp"

namespace openpuzzle {

class Database;

class DaemonRunner {
public:
  explicit DaemonRunner(Database& database);

  int run(int ticks = 3);
  void stop();

private:
  void loadWorkers();
  void heartbeatWorkers();
  void synchronizeWorkers();
  void tick();

  Database& database_;
  WorkerAgentRegistry workers_;
  DefaultSchedulingPolicy schedulingPolicy_;

  bool running_ = false;
  int tickCount_ = 0;
};

} // namespace openpuzzle
