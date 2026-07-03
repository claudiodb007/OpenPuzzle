#include "openpuzzle/performance/BenchmarkRunner.hpp"

#include "openpuzzle/core/ExecutionManager.hpp"

namespace openpuzzle {

BenchmarkResult
BenchmarkRunner::run(const BenchmarkConfiguration &configuration,
                     const ExecutionContext &context, int maxSeconds) const {
  BenchmarkResult result;
  result.configuration = configuration;

  ExecutionManager manager;
  auto executionResult = manager.run(context, maxSeconds);

  result.success =
      executionResult.success || executionResult.averageSpeed > 0.0;

  result.speedMKeys = executionResult.averageSpeed;
  result.averageSpeed = executionResult.averageSpeed;

  if (executionResult.averageSpeed > 0.0) {
    result.minimumSpeed = executionResult.averageSpeed;
    result.maximumSpeed = executionResult.averageSpeed;
    result.samples = 1;
  }

  return result;
}

} // namespace openpuzzle
