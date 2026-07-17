#pragma once

#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/scheduler/DefaultSchedulingPolicy.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <chrono>
#include <string>

namespace openpuzzle {

class Database;

class DaemonRunner {
public:
  explicit DaemonRunner(Database& database);

  DaemonRunner(
      Database& database,
      std::string serverUrl,
      int syncIntervalSeconds);

  int run(int ticks = 3);
  void stop();

private:
  void loadWorkers();
  void registerClient();
  void tick();
  void syncClientExecution();

  Database& database_;
  WorkerAgentRegistry workers_;
  DefaultSchedulingPolicy schedulingPolicy_;
  EngineManager engineManager_;

  bool running_ = false;
  int tickCount_ = 0;

  std::string serverUrl_ =
      "https://claudiodb.com";

  std::chrono::seconds syncInterval_ =
      std::chrono::seconds(60);

  bool hasSynced_ = false;

  std::chrono::steady_clock::time_point
      lastSyncAt_;
};

} // namespace openpuzzle
