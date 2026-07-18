#include "openpuzzle/core/PosixProcessRunner.hpp"

#include <chrono>
#include <string>

using namespace openpuzzle;

int main() {
  PosixProcessRunner runner;

  bool receivedProgress = false;

  const auto startedAt =
      std::chrono::steady_clock::now();

  const auto result =
      runner.run(
          "printf 'progress\\r'; sleep 3",
          [&](const std::string &line) {
            if (line == "progress") {
              receivedProgress = true;
            }
          },
          1);

  const auto elapsed =
      std::chrono::duration_cast<
          std::chrono::milliseconds>(
          std::chrono::steady_clock::now() -
          startedAt);

  if (!result.started)
    return 1;

  if (result.exitCode != 124)
    return 2;

  if (!receivedProgress)
    return 3;

  if (elapsed.count() >= 2500)
    return 4;

  return 0;
}
