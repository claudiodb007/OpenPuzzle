#pragma once

namespace openpuzzle {

struct BenchmarkResult {
  int blocks = 0;
  int threads = 0;
  int points = 0;

  double speedMKeys = 0.0;
  bool success = false;
};

} // namespace openpuzzle
