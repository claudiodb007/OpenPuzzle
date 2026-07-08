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

    std::cout << "Execution " << args[1] << "\n";
    std::cout << "Not loaded in runtime yet.\n";
    return 0;
  }

  std::cerr << "Unknown execution command\n";
  return 1;
}

} // namespace openpuzzle
