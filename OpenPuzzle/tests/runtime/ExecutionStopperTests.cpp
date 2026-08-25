#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"

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

  {
    std::ofstream out(
        workspace / "process.boot_id");

    out
        << client::ClientStateStore::currentBootId()
        << "\n";
  }

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

  /*
   * Um PID válido com boot_id diferente nunca pode
   * receber sinal através do ExecutionStopper.
   */
  auto staleWorkspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-stopper-stale-" +
       std::to_string(getpid()));

  std::filesystem::create_directories(
      staleWorkspace);

  {
    std::ofstream pidOut(
        staleWorkspace / "process.pid");

    pidOut
        << static_cast<int>(getpid())
        << "\n";
  }

  {
    std::ofstream bootOut(
        staleWorkspace / "process.boot_id");

    bootOut
        << "00000000-0000-0000-0000-000000000000"
        << "\n";
  }

  if (stopper.stop(
          staleWorkspace.string())) {
    std::cerr
        << "ExecutionStopper accepted stale boot identity\n";

    return 1;
  }

  if (!exists(
          static_cast<int>(getpid()))) {
    std::cerr
        << "Current process was incorrectly signalled\n";

    return 1;
  }

  std::filesystem::remove_all(
      staleWorkspace);

  return 0;
}
