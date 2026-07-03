#include "openpuzzle/performance/BenchmarkRunner.hpp"

#include "openpuzzle/core/ExecutionManager.hpp"

#include <cstddef>

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
    const auto startIndex = executionResult.speedSamples.size() > 1
                                ? std::size_t{1}
                                : std::size_t{0};

    double sum = 0.0;

    result.minimumSpeed = executionResult.speedSamples[startIndex];
    result.maximumSpeed = executionResult.speedSamples[startIndex];

    for (std::size_t i = startIndex; i < executionResult.speedSamples.size();
         ++i) {
      const double speed = executionResult.speedSamples[i];

      if (speed <= 0.0)
        continue;

      sum += speed;

      if (speed < result.minimumSpeed)
        result.minimumSpeed = speed;

      if (speed > result.maximumSpeed)
        result.maximumSpeed = speed;
    }

    result.samples = 0;
    for (std::size_t i = startIndex; i < executionResult.speedSamples.size();
         ++i) {
      if (executionResult.speedSamples[i] > 0.0)
        result.samples++;
    }

    if (result.samples > 0) {
      result.averageSpeed = sum / result.samples;
    }
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
