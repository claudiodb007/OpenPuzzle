#pragma once

#include <string>

namespace openpuzzle {

struct StartExecutionRequest {
  int executionId = 0;
  int puzzleId = 0;
  int jobId = 0;
  int rangeId = 0;

  std::string engine;
  std::string backend;
  std::string workspace;
  std::string command;

  bool echoOutput = true;
};

} // namespace openpuzzle
