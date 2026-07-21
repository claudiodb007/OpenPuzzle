#pragma once

#include <functional>

namespace openpuzzle {

struct RunBenchmarkPreparationDependencies {
  std::function<bool()> hasValidProfile;
  std::function<int()> runBenchmark;
};

class RunBenchmarkPreparation {
public:
  explicit RunBenchmarkPreparation(
      RunBenchmarkPreparationDependencies dependencies);

  bool ensureProfile() const;

private:
  RunBenchmarkPreparationDependencies dependencies_;
};

} // namespace openpuzzle
