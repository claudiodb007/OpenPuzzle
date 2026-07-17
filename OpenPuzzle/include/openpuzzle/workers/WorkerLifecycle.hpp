#pragma once

#include "openpuzzle/runtime/ExecutionProcessMonitor.hpp"

namespace openpuzzle {

class Database;
class WorkerAgentRegistry;

class WorkerLifecycle {
public:
  WorkerLifecycle(
      Database& database,
      WorkerAgentRegistry& workers);

  void refreshHeartbeats(int timeoutSeconds = 30);

  ExecutionProcessMonitorSummary monitorExecutions();

private:
  void synchronizeCompletedExecutions();

  Database& database_;
  WorkerAgentRegistry& workers_;
};

} // namespace openpuzzle
