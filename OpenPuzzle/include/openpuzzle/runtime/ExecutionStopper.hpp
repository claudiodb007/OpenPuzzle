#pragma once

#include <string>

namespace openpuzzle {

class ExecutionStopper {
public:
  bool stop(const std::string& workspace) const;

private:
  static int readPid(const std::string& workspace);
  static bool processExists(int pid);
  static bool signalProcessGroup(int pid, int signal);
  static bool signalProcess(int pid, int signal);
};

} // namespace openpuzzle
