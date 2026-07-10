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

  int device = 0;
  int blocks = 0;
  int threads = 0;
  int points = 0;

  std::string workspace;
  std::string command;

  bool echoOutput = true;
};

} // namespace openpuzzle
