#pragma once

#include "openpuzzle/client/ClientHeartbeatService.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"

#include <chrono>
#include <functional>
#include <string>
#include <utility>

namespace openpuzzle {

enum class ClientIterationStatus {
  Completed,
  Unavailable,
  Retry,
  SolutionFound,
  Failed
};

struct ClientIterationResult {
  ClientIterationStatus status =
      ClientIterationStatus::Completed;

  int exitCode = 0;
  std::string message;

  ClientIterationResult() = default;

  ClientIterationResult(int code)
      : status(
            code == 0
                ? ClientIterationStatus::Completed
                : ClientIterationStatus::Failed),
        exitCode(code) {}

  static ClientIterationResult unavailable(
      std::string message) {
    ClientIterationResult result;
    result.status =
        ClientIterationStatus::Unavailable;
    result.message =
        std::move(message);
    return result;
  }

  static ClientIterationResult
  solutionFound(
      std::string message = {}) {
    ClientIterationResult result;
    result.status =
        ClientIterationStatus::SolutionFound;
    result.message =
        std::move(message);
    return result;
  }

  static ClientIterationResult retry(
      std::string message) {
    ClientIterationResult result;
    result.status =
        ClientIterationStatus::Retry;
    result.message =
        std::move(message);
    return result;
  }
};

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
      std::string &error)> reportSolution;

  std::function<
      client::AssignmentUploadStatus(
          const std::string &serverUrl,
          const std::string &assignmentId,
          const std::string &clientId,
          int exitCode,
          const std::string &status,
          const std::string &keysChecked,
          std::string &error)>
      finalizeAssignment;

  std::function<std::string(
      const std::string &workspace)>
      finalKeysChecked;

  std::function<bool()> removeState;

  std::function<bool()> hasState;

  std::function<bool()> acquireRuntime;
  std::function<void()> releaseRuntime;

  std::function<bool()> stopRequested;
  std::function<bool()> safeStopRequested;
  std::function<bool()> clearSafeStop;
  std::function<void()> prepareSignals;

  std::function<void(
      std::chrono::seconds duration)> sleep;
};

class ClientRuntime {
public:
  static constexpr int
      SolutionFoundExitCode = 10;

  ClientRuntime();

  explicit ClientRuntime(
      ClientRuntimeDependencies dependencies);

  int runContinuous(
      const std::string &serverUrl,
      const std::function<ClientIterationResult()> &
          executeAssignment) const;

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
