#pragma once

#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <string>

namespace openpuzzle {

struct ExecutionPlan {

  int workerId = 0;

  int jobId = 0;

  std::string engine;

  std::string backend;

  WorkerEngineCapability capability;

  double expectedSpeedMkeys = 0.0;

  bool valid = false;
};

} // namespace openpuzzle
