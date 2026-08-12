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
      << "OpenPuzzle first run\n"
      << "--------------------\n"
      << "[1/4] Hardware and engine... ready\n"
      << "[2/4] GPU profile.......... not found\n"
      << "[3/4] Safe benchmark....... starting\n"
      << "[4/4] Server contact....... waiting\n"
      << "The benchmark may take several minutes.\n"
      << "No assignment will be requested until it succeeds.\n\n";

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
        << "[4/4] Server contact not started\n"
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
        << "[4/4] Server contact not started\n"
        << "Action 1........... run: openpuzzle doctor\n"
        << "Action 2........... run: openpuzzle benchmark --real --auto\n";

    return false;
  }

  std::cout
      << "[3/4] Safe benchmark....... complete\n"
      << "      GPU profile.......... saved\n"
      << "[4/4] Server contact....... ready\n"
      << "Setup complete. OpenPuzzle will now request work.\n\n";

  return true;
}

} // namespace openpuzzle
