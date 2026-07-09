#include "openpuzzle/services/ExecutionService.hpp"

#include "openpuzzle/core/ExecutionRecord.hpp"
#include "openpuzzle/database/Database.hpp"

#include <iomanip>
#include <iostream>

namespace openpuzzle {

int ExecutionService::execute(const std::vector<std::string>& args) {
  if (args.empty()) {
    std::cerr << "Usage: OpenPuzzle execution list|show <id>\n";
    return 1;
  }

  if (args[0] == "list") {
    auto executions = database_.listExecutions();

    std::cout << "ID   JOB   PUZZLE   RANGE   STATUS\n";
    std::cout << "-------------------------------------------\n";

    if (executions.empty()) {
      std::cout << "(no executions)\n";
      return 0;
    }

    for (const auto& execution : executions) {
      std::cout
          << std::left
          << std::setw(5) << execution.executionId
          << std::setw(6) << execution.jobId
          << std::setw(8) << execution.puzzleId
          << std::setw(8) << execution.rangeId
          << ExecutionRecord::statusToString(execution.status)
          << "\n";
    }

    return 0;
  }

  if (args[0] == "show") {

    if (args.size() < 2) {
        std::cerr << "Usage: OpenPuzzle execution show <id>\n";
        return 1;
    }

    int executionId = std::stoi(args[1]);

    auto execution = database_.getExecution(executionId);

    if (!execution) {
        std::cerr << "Execution not found: "
                  << executionId << "\n";
        return 1;
    }

    std::cout << "Execution\n";
    std::cout << "---------\n";

    std::cout << "ID................. "
              << execution->executionId << "\n";

    std::cout << "Job................ "
              << execution->jobId << "\n";

    std::cout << "Puzzle............. "
              << execution->puzzleId << "\n";

    std::cout << "Range.............. "
              << execution->rangeId << "\n";

    std::cout << "Status............. "
              << ExecutionRecord::statusToString(execution->status)
              << "\n";

    std::cout << "Workspace.......... "
              << execution->workspace << "\n";

    std::cout << "\nCommand\n";
    std::cout << "-------\n";

    std::cout << execution->command << "\n";

    return 0;
}

  std::cerr << "Unknown execution command\n";
  return 1;
}

} // namespace openpuzzle
