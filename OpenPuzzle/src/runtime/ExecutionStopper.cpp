#include "openpuzzle/runtime/ExecutionStopper.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace openpuzzle {

int ExecutionStopper::readPid(const std::string& workspace) {
  auto pidFile = std::filesystem::path(workspace) / "process.pid";

  std::ifstream in(pidFile);

  if (!in.is_open()) {
    return 0;
  }

  int pid = 0;
  in >> pid;

  return pid;
}

bool ExecutionStopper::stop(const std::string& workspace) const {
  int pid = readPid(workspace);

  if (pid <= 0) {
    return false;
  }

  if (kill(pid, SIGTERM) == 0) {
    return true;
  }

  return false;
}

} // namespace openpuzzle
