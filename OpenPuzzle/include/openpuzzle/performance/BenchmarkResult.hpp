#pragma once

#include "openpuzzle/performance/BenchmarkConfiguration.hpp"

namespace openpuzzle {

struct BenchmarkResult {
  BenchmarkConfiguration configuration;

  double speedMKeys = 0.0;
  double averageSpeed = 0.0;
  double minimumSpeed = 0.0;
  double maximumSpeed = 0.0;

  int samples = 0;

  bool success = false;
};

} // namespace openpuzzle
