#include "openpuzzle/core/PosixProcessRunner.hpp"

#include <chrono>
#include <string>

using namespace openpuzzle;

int main() {
  PosixProcessRunner runner;

  bool receivedProgress = false;
  bool receivedShutdownRecord = false;

  const auto startedAt =
      std::chrono::steady_clock::now();

  const auto result =
      runner.run(
          "trap \"printf 'stopped\\n'; exit 0\" TERM; "
          "printf 'progress\\r'; while :; do sleep 1; done",
          [&](const std::string &line) {
            if (line == "progress") {
              receivedProgress = true;
            }
            if (line == "stopped") {
              receivedShutdownRecord = true;
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

  if (!receivedShutdownRecord)
    return 5;

  if (elapsed.count() >= 2500)
    return 4;

  return 0;
}
