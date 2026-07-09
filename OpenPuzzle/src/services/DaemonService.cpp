#include "openpuzzle/services/DaemonService.hpp"

#include "openpuzzle/runtime/DaemonRunner.hpp"

#include <iostream>

namespace openpuzzle {

int DaemonService::execute(const std::vector<std::string>& args) {
  int ticks = 3;

  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == "--ticks") {
      ticks = std::stoi(args[i + 1]);
    }
  }

  DaemonRunner runner;
  return runner.run(ticks);
}

} // namespace openpuzzle
