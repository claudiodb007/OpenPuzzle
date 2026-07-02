#pragma once

namespace openpuzzle {

struct ProcessHandle {
  int pid = -1;
  bool started = false;
  bool running = false;
  int exitCode = -1;
  bool timedOut = false;
  bool terminatedBySignal = false;
};

} // namespace openpuzzle
