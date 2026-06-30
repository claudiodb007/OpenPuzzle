#pragma once

#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/core/ProcessRunner.hpp"

#include <string>

namespace openpuzzle {

struct ExecutionSummary {
  bool started = false;
  int exitCode = -1;

  int totalLines = 0;
  int speedEvents = 0;
  int errorEvents = 0;
  int foundEvents = 0;
  int finishedEvents = 0;

  double lastSpeedMKeys = 0.0;
};

class ExecutionManager {
public:
  ExecutionSummary runCommand(const std::string &command,
                              bool echoOutput = true) const;
  ExecutionResult run(const ExecutionContext &context) const;
};

} // namespace openpuzzle
