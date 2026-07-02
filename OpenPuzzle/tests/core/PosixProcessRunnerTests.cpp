#include "openpuzzle/core/PosixProcessRunner.hpp"

#include <string>

using namespace openpuzzle;

int main() {
  PosixProcessRunner runner;

  std::string output;

  auto result = runner.run("printf 'hello\\n'",
                           [&](const std::string &line) { output = line; });

  if (!result.started)
    return 1;
  if (result.exitCode != 0)
    return 2;
  if (output != "hello")
    return 3;

  return 0;
}
