#include "openpuzzle/core/ProcessRunner.hpp"

using namespace openpuzzle;

int main() {
  ProcessRunner runner;

  int lines = 0;

  auto result = runner.run("printf 'alpha\\nbeta\\n'",
                           [&](const std::string &) { lines++; });

  if (!result.started) {
    return 1;
  }

  if (result.exitCode != 0) {
    return 2;
  }

  if (lines != 2) {
    return 3;
  }

  return 0;
}
