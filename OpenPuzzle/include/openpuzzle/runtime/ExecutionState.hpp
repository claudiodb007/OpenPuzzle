#pragma once

#include <string>

namespace openpuzzle {

enum class RuntimeExecutionStatus {
  Pending,
  Running,
  Finished,
  Failed,
  Stopped
};

struct ExecutionState {
  int executionId = 0;
  int puzzleId = 0;
  int jobId = 0;
  int rangeId = 0;

  std::string engine;
  std::string backend;
  std::string workspace;
  std::string command;

  RuntimeExecutionStatus status = RuntimeExecutionStatus::Pending;

  double currentSpeed = 0.0;
  double averageSpeed = 0.0;

  std::string keysChecked = "0";

  bool keyFound = false;
  std::string privateKey;

  int exitCode = -1;
};

} // namespace openpuzzle
