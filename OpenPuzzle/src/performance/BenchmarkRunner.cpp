#include "openpuzzle/performance/BenchmarkRunner.hpp"

#include "openpuzzle/core/ExecutionManager.hpp"

namespace openpuzzle {

BenchmarkResult
BenchmarkRunner::run(const BenchmarkConfiguration &configuration,
                     const ExecutionContext &context) const {
  BenchmarkResult result;
  result.configuration = configuration;

  ExecutionManager manager;
  auto executionResult = manager.run(context);

  result.success = executionResult.success;
  result.speedMKeys = executionResult.averageSpeed;

  return result;
}

} // namespace openpuzzle
