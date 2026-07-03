#include "openpuzzle/performance/BenchmarkRunner.hpp"

#include "openpuzzle/core/ExecutionManager.hpp"

namespace openpuzzle {

BenchmarkResult
BenchmarkRunner::run(const BenchmarkConfiguration &configuration,
                     const ExecutionContext &context, int maxSeconds,
                     int maxSamples) const {
  BenchmarkResult result;
  result.configuration = configuration;

  ExecutionManager manager;
  auto executionResult = manager.run(context, maxSeconds, maxSamples);

  result.success =
      executionResult.success || executionResult.averageSpeed > 0.0;

  if (!executionResult.speedSamples.empty()) {
    double sum = 0.0;

    result.minimumSpeed = executionResult.speedSamples.front();
    result.maximumSpeed = executionResult.speedSamples.front();

    for (double speed : executionResult.speedSamples) {
      sum += speed;

      if (speed < result.minimumSpeed)
        result.minimumSpeed = speed;

      if (speed > result.maximumSpeed)
        result.maximumSpeed = speed;
    }

    result.samples = static_cast<int>(executionResult.speedSamples.size());
    result.averageSpeed = sum / result.samples;
    result.speedMKeys = result.averageSpeed;
  } else {
    result.speedMKeys = executionResult.averageSpeed;
    result.averageSpeed = executionResult.averageSpeed;

    if (executionResult.averageSpeed > 0.0) {
      result.minimumSpeed = executionResult.averageSpeed;
      result.maximumSpeed = executionResult.averageSpeed;
      result.samples = 1;
    }
  }

  return result;
}

} // namespace openpuzzle
