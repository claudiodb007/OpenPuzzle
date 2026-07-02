#include "openpuzzle/core/ProcessHandle.hpp"

using namespace openpuzzle;

int main() {
  ProcessHandle handle;

  if (handle.pid != -1)
    return 1;
  if (handle.started)
    return 2;
  if (handle.running)
    return 3;
  if (handle.exitCode != -1)
    return 4;
  if (handle.timedOut)
    return 5;
  if (handle.terminatedBySignal)
    return 6;

  return 0;
}
