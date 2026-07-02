#pragma once

#include "openpuzzle/performance/BenchmarkConfiguration.hpp"

namespace openpuzzle {

struct BenchmarkResult {
  BenchmarkConfiguration configuration;

  double speedMKeys = 0.0;
  bool success = false;
};

} // namespace openpuzzle
