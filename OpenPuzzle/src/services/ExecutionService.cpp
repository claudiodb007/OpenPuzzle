#include "openpuzzle/services/ExecutionService.hpp"

#include <iostream>

namespace openpuzzle {

int ExecutionService::execute(const std::vector<std::string>& args) {
  if (args.empty()) {
    std::cerr << "Usage: OpenPuzzle execution list|show <id>\n";
    return 1;
  }

  if (args[0] == "list") {
    std::cout << "ID   JOB   RANGE   ENGINE   STATUS\n";
    std::cout << "-----------------------------------\n";
    std::cout << "(no runtime executions loaded)\n";
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
