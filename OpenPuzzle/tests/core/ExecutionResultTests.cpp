#include "openpuzzle/core/ExecutionResult.hpp"

using namespace openpuzzle;

int main() {
  ExecutionResult result;

  result.success = true;
  result.exitCode = 0;
  result.linesRead = 123;
  result.averageSpeed = 1334.62;
  result.keyFound = true;
  result.privateKey = "private-key";

  if (!result.success)
    return 1;
  if (result.exitCode != 0)
    return 2;
  if (result.linesRead != 123)
    return 3;
  if (result.averageSpeed != 1334.62)
    return 4;
  if (!result.keyFound)
    return 5;
  if (result.privateKey != "private-key")
    return 6;

  return 0;
}
