#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/runtime/ExecutionProgress.hpp"

#include <string>

namespace openpuzzle::client {

struct ExecutionSyncResult {
  bool hasState = false;

  ClientExecutionState state;

  bool running = false;

  bool hasProgress = false;
  ExecutionProgress progress;

  bool progressUploaded = false;
  std::string progressError;

  bool hasExitCode = false;
  int exitCode = 0;

  bool completionUploaded = false;
  bool stateRemoved = false;
  std::string completionError;
};

class ExecutionSyncService {
public:
  ExecutionSyncResult tick(
      const std::string& serverUrl) const;

private:
  static bool processExists(
      int pid);

  static bool readExitCode(
      const std::string& workspace,
      int& exitCode);

  static bool readLatestProgress(
      const std::string& workspace,
      ExecutionProgress& progress);
};

} // namespace openpuzzle::client
