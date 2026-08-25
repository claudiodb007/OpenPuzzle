#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"

#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <thread>
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

static std::string readBootId(
    const std::string& workspace) {
  const auto path =
      std::filesystem::path(workspace) /
      "process.boot_id";

  std::ifstream input(path);

  if (!input.is_open()) {
    return {};
  }

  std::string bootId;

  if (!(input >> bootId)) {
    return {};
  }

  return bootId;
}

static bool processIdentityMatches(
    const std::string& workspace,
    int pid) {
  const auto storedBootId =
      readBootId(workspace);

  const auto currentBootId =
      client::ClientStateStore::
          currentBootId();

  if (storedBootId.empty() ||
      currentBootId.empty() ||
      storedBootId != currentBootId) {
    return false;
  }

  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
}

bool ExecutionStopper::processExists(int pid) {
  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
}

bool ExecutionStopper::signalProcessGroup(int pid, int signal) {
  if (pid <= 0) {
    return false;
  }

  return kill(-pid, signal) == 0 || errno == ESRCH;
}

bool ExecutionStopper::signalProcess(int pid, int signal) {
  if (pid <= 0) {
    return false;
  }

  return kill(pid, signal) == 0 || errno == ESRCH;
}

bool ExecutionStopper::stop(const std::string& workspace) const {
  int pid = readPid(workspace);

  if (pid <= 0) {
    return false;
  }

  if (!processIdentityMatches(
          workspace,
          pid)) {
    return false;
  }

  bool signalled = false;

  signalled = signalProcessGroup(pid, SIGTERM) || signalled;
  signalled = signalProcess(pid, SIGTERM) || signalled;

  for (int i = 0; i < 20; ++i) {
    if (!processExists(pid)) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  signalled = signalProcessGroup(pid, SIGKILL) || signalled;
  signalled = signalProcess(pid, SIGKILL) || signalled;

  for (int i = 0; i < 10; ++i) {
    if (!processExists(pid)) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return signalled;
}

} // namespace openpuzzle
