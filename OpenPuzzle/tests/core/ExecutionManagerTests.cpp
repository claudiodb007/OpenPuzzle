#include "openpuzzle/core/ExecutionManager.hpp"

using namespace openpuzzle;

int main() {
  ExecutionManager manager;

  auto result = manager.runCommand(
      "printf '[Info] Starting at: 400000000000000000\\n[Info] 1334.62 "
      "MKey/s\\nFinished\\n'",
      false);

  if (!result.started)
    return 1;
  if (result.exitCode != 0)
    return 2;
  if (result.totalLines != 3)
    return 3;
  if (result.speedEvents != 1)
    return 4;
  if (result.lastSpeedMKeys != 1334.62)
    return 5;
  if (result.finishedEvents != 1)
    return 6;
  if (result.errorEvents != 0)
    return 7;
  if (result.foundEvents != 0)
    return 8;

  return 0;
}
