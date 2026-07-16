#pragma once

#include "openpuzzle/client/ClientHeartbeatService.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"

#include <chrono>
#include <functional>
#include <string>

namespace openpuzzle {

struct ClientRuntimeDependencies {
  std::function<client::ExecutionSyncResult(
      const std::string &serverUrl)> sync;

  std::function<client::ClientHeartbeatResult(
      const std::string &serverUrl)> heartbeat;

  std::function<bool(
      const std::string &workspace)> stopExecution;

  std::function<bool(
      const std::string &serverUrl,
      const std::string &assignmentId,
      const std::string &clientId,
      std::string &error)> cancelAssignment;

  std::function<bool()> removeState;
  std::function<bool()> stopRequested;
  std::function<void()> prepareSignals;

  std::function<void(
      std::chrono::seconds duration)> sleep;
};

class ClientRuntime {
public:
  ClientRuntime();

  explicit ClientRuntime(
      ClientRuntimeDependencies dependencies);

  int run(const std::string &serverUrl,
          const std::string &assignmentId,
          const std::string &clientId,
          const std::string &workspace) const;

private:
  ClientRuntimeDependencies dependencies_;

  bool sleepInterruptibly(
      std::chrono::seconds duration) const;

  static ClientRuntimeDependencies
  productionDependencies();
};

} // namespace openpuzzle
