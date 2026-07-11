#pragma once

#include "openpuzzle/runtime/ExecutionProcessMonitor.hpp"
#include "openpuzzle/runtime/SchedulerDecision.hpp"

namespace openpuzzle {

struct RuntimeTickResult {
  SchedulerDecision decision;

  bool dispatched = false;
  bool launchSuccess = false;
  int exitCode = 0;

  ExecutionProcessMonitorSummary monitor;
};

} // namespace openpuzzle
