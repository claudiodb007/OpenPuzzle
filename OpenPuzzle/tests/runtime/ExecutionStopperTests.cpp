#include "openpuzzle/runtime/ExecutionStopper.hpp"

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

using namespace openpuzzle;

static bool exists(int pid) {
  if (pid <= 0) {
    return false;
  }

  return kill(pid, 0) == 0;
}

int main() {
  auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-stopper-" + std::to_string(getpid()));

  std::filesystem::create_directories(workspace);

  std::string command =
      "setsid sh -c 'sleep 9999' >/dev/null 2>&1 & echo $! > " +
      (workspace / "process.pid").string();

  int rc = std::system(command.c_str());

  if (rc != 0) {
    std::cerr << "Failed to start test process\n";
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::ifstream in(workspace / "process.pid");
  int pid = 0;
  in >> pid;

  if (pid <= 0 || !exists(pid)) {
    std::cerr << "Test process was not started\n";
    return 1;
  }

  ExecutionStopper stopper;

  if (!stopper.stop(workspace.string())) {
    std::cerr << "ExecutionStopper returned false\n";
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  if (exists(pid)) {
    std::cerr << "Process still exists after stop\n";
    return 1;
  }

  std::filesystem::remove_all(workspace);

  return 0;
}
