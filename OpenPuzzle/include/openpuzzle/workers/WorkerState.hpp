#pragma once

#include "openpuzzle/workers/WorkerCapabilities.hpp"

#include <string>

namespace openpuzzle {

enum class WorkerStatus {
  Offline,
  Online,
  Busy,
  Error
};

struct WorkerState {
  int workerId = 0;

  std::string name;
  std::string hostname;

  WorkerStatus status = WorkerStatus::Offline;

  std::string engine;
  std::string backend;

  WorkerCapabilities capabilities;


  int gpuDevice = 0;

  double lastSpeed = 0.0;
};

} // namespace openpuzzle
