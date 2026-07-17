#pragma once

#include <string>

namespace openpuzzle {

struct WorkerCapabilities {
  bool cuda = false;
  bool opencl = false;
  bool cpu = true;

  std::string gpuName;
  std::string driverVersion;

  int gpuDevice = 0;
  int vramMb = 0;

  double lastBenchmarkSpeed = 0.0;
};

} // namespace openpuzzle
