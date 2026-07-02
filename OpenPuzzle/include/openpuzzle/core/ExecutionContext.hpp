#pragma once

#include <functional>
#include <string>

namespace openpuzzle {

struct ExecutionResult;

struct ExecutionContext {
  int executionId = 0;

  int puzzleId = 0;
  int jobId = 0;
  int rangeId = 0;

  std::string engine;
  std::string workspace;
  std::string command;

  bool echoOutput = true;

  std::function<void(const ExecutionResult &)> onProgress;
};

} // namespace openpuzzle
