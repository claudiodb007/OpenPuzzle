#include "openpuzzle/core/commands/BenchmarkCommand.hpp"

#include <iostream>

namespace openpuzzle {

int BenchmarkCommand::run(const std::vector<std::string> &args) const {
  (void)args;

  std::cout << "BenchmarkCommand skeleton\n";
  return 0;
}

} // namespace openpuzzle
