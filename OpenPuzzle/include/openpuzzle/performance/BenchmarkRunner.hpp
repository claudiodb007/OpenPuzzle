#pragma once

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/performance/BenchmarkConfiguration.hpp"
#include "openpuzzle/performance/BenchmarkResult.hpp"

namespace openpuzzle {

class BenchmarkRunner {
public:
  BenchmarkResult run(const BenchmarkConfiguration &configuration,
                      const ExecutionContext &context,
                      int maxSeconds = 0) const;
};

} // namespace openpuzzle
