#pragma once

#include <functional>
#include <string>

namespace openpuzzle {

struct RunBenchmarkPreparationDependencies {
  std::function<bool()> hasValidProfile;
  std::function<int()> runBenchmark;
  std::string benchmarkCommand = "openpuzzle benchmark --real --auto";
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
