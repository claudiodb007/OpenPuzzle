#include "openpuzzle/core/PosixProcessRunner.hpp"

using namespace openpuzzle;

int main() {
  PosixProcessRunner runner;

  auto result =
      runner.run("printf 'start\\n'; sleep 3; printf 'end\\n'", nullptr, 1);

  if (!result.started)
    return 1;
  if (result.exitCode != 124)
    return 2;

  return 0;
}
