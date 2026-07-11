#pragma once

#include "openpuzzle/runtime/ExecutionPlan.hpp"
#include "openpuzzle/runtime/ExecutionProcessMonitor.hpp"
#include "openpuzzle/runtime/SchedulerDecision.hpp"

#include <optional>

namespace openpuzzle {

struct RuntimeTickResult {
  std::optional<ExecutionPlan> plan;

  // Compatibilidade temporária com o daemon e testes antigos.
  SchedulerDecision decision;

  bool dispatched = false;
  bool launchSuccess = false;
  int exitCode = 0;

  ExecutionProcessMonitorSummary monitor;
};

} // namespace openpuzzle
