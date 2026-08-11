#include "openpuzzle/runtime/RunBenchmarkPreparation.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace openpuzzle {

RunBenchmarkPreparation::RunBenchmarkPreparation(
    RunBenchmarkPreparationDependencies dependencies)
    : dependencies_(std::move(dependencies)) {
  if (!dependencies_.hasValidProfile ||
      !dependencies_.runBenchmark) {
    throw std::invalid_argument(
        "Incomplete RunBenchmarkPreparation dependencies");
  }
}

bool RunBenchmarkPreparation::ensureProfile() const {
  if (dependencies_.hasValidProfile()) {
    return true;
  }

  std::cout
      << "First-use GPU benchmark\n"
      << "-----------------------\n"
      << "GPU profile........ not found\n"
      << "Benchmark.......... starting\n"
      << "Server contact..... none\n"
      << "This may take several minutes.\n\n";

  const int result =
      dependencies_.runBenchmark();

  if (result != 0) {
    std::cerr
        << "Automatic benchmark failed\n"
        << "--------------------------\n"
        << "Error code......... OP-BENCH-001\n"
        << "Problem............ benchmark command returned "
        << result
        << '\n'
        << "Assignment......... not requested\n"
        << "Action 1........... run: openpuzzle doctor\n"
        << "Action 2........... run: openpuzzle benchmark --real --auto\n";

    return false;
  }

  if (!dependencies_.hasValidProfile()) {
    std::cerr
        << "Automatic benchmark failed\n"
        << "--------------------------\n"
        << "Error code......... OP-BENCH-002\n"
        << "Problem............ benchmark did not save a valid GPU profile\n"
        << "Assignment......... not requested\n"
        << "Action 1........... run: openpuzzle doctor\n"
        << "Action 2........... run: openpuzzle benchmark --real --auto\n";

    return false;
  }

  std::cout
      << "First-use setup.... complete\n"
      << "GPU profile........ saved\n\n";

  return true;
}

} // namespace openpuzzle
