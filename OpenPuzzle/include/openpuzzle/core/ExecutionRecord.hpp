#pragma once

#include <string>

namespace openpuzzle {

enum class ExecutionRecordStatus {
  Created,
  Running,
  Finished,
  Failed,
  Cancelled
};

struct ExecutionRecord {
  int executionId = 0;
  int jobId = 0;
  int puzzleId = 0;
  int rangeId = 0;

  std::string engine;
  std::string workspace;
  std::string command;

  ExecutionRecordStatus status = ExecutionRecordStatus::Created;

  static std::string statusToString(ExecutionRecordStatus status);
};

} // namespace openpuzzle
