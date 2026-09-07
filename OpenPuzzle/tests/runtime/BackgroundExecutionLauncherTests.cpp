#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"

#include "openpuzzle/runtime/ExecutionStopper.hpp"

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

using namespace openpuzzle;

namespace {

std::string shellQuote(
    const std::string &value) {
  std::string quoted = "'";

  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }

  quoted += '\'';
  return quoted;
}

std::string readFile(
    const std::filesystem::path& path) {
  std::ifstream input(path);

  std::ostringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
}

void assertPrivateFile(
    const std::filesystem::path &path) {
  const auto permissions =
      std::filesystem::status(
          path).permissions();

  assert(
      (permissions &
       std::filesystem::perms::owner_read) !=
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::owner_write) !=
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::group_all) ==
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::others_all) ==
      std::filesystem::perms::none);
}

bool waitForFile(
    const std::filesystem::path& path,
    std::chrono::milliseconds timeout) {
  const auto deadline =
      std::chrono::steady_clock::now() +
      timeout;

  while (
      std::chrono::steady_clock::now() <
      deadline) {
    if (std::filesystem::is_regular_file(
            path)) {
      return true;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(25));
  }

  return std::filesystem::is_regular_file(
      path);
}

bool processExists(int pid) {
  return
      pid > 0 &&
      (::kill(pid, 0) == 0 ||
       errno == EPERM);
}

} // namespace

int main() {
  const auto workspace =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-background launcher-'quoted'-" +
          std::to_string(getpid())
      );

  std::filesystem::remove_all(
      workspace);

  BackgroundExecutionLauncher launcher;

  /*
   * Um workspace com identidade válida de um processo vivo
   * não pode receber uma segunda execução.
   */
  const auto duplicateWorkspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-background-duplicate-" +
       std::to_string(getpid()));

  std::filesystem::remove_all(
      duplicateWorkspace);

  StartExecutionRequest duplicateRequest;
  duplicateRequest.executionId = 76;
  duplicateRequest.workspace =
      duplicateWorkspace.string();
  duplicateRequest.command = "sleep 9999";

  const auto duplicateHandle =
      launcher.start(
          duplicateRequest);

  bool duplicateRejected = false;

  try {
    launcher.start(
        duplicateRequest);
  } catch (...) {
    duplicateRejected = true;
  }

  assert(duplicateRejected);

  {
    std::ifstream pidInput(
        duplicateWorkspace /
        "process.pid");

    int recordedPid = 0;
    assert(pidInput >> recordedPid);
    assert(recordedPid == duplicateHandle.pid);
  }

  ExecutionStopper duplicateStopper;

  assert(
      duplicateStopper.stop(
          duplicateWorkspace.string()));

  std::filesystem::remove_all(
      duplicateWorkspace);

  const auto stopWorkspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-background-stop-" +
       std::to_string(getpid()));

  std::filesystem::remove_all(
      stopWorkspace);

  StartExecutionRequest stopRequest;
  stopRequest.executionId = 78;
  stopRequest.workspace =
      stopWorkspace.string();
  stopRequest.command =
      "sleep 9999 & child=$!; "
      "printf '%s\\n' \"$child\" > " +
      shellQuote(
          stopWorkspace / "child.pid") +
      "; wait \"$child\"";

  const auto stopHandle =
      launcher.start(
          stopRequest);

  const auto childPath =
      stopWorkspace / "child.pid";

  assert(
      waitForFile(
          childPath,
          std::chrono::seconds(2)));

  int childPid = 0;

  {
    std::ifstream input(childPath);
    input >> childPid;
  }

  assert(childPid > 0);
  assert(processExists(stopHandle.pid));
  assert(processExists(childPid));
  assert(
      ::getpgid(stopHandle.pid) ==
      stopHandle.pid);

  ExecutionStopper stopper;
  assert(
      stopper.stop(
          stopWorkspace.string()));

  for (int attempt = 0;
       attempt < 40 &&
       processExists(childPid);
       ++attempt) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));
  }

  assert(!processExists(stopHandle.pid));
  assert(!processExists(childPid));

  std::filesystem::remove_all(
      stopWorkspace);

  StartExecutionRequest request;

  request.executionId = 77;

  request.workspace =
      workspace.string();

  request.command =
      "if [ 'quoted value' = 'quoted value' ]; then "
      "printf '%s\\n' 'stdout-line'; "
      "printf '%s\\n' 'stderr-line' >&2; "
      "printf '%s\\n' 'synthetic-found' > " +
      shellQuote(workspace / "found.txt") +
      "; else exit 66; fi; sleep 1; exit 7";

  const auto handle =
      launcher.start(
          request);

  assert(handle.executionId == 77);
  assert(handle.pid > 0);

  assert(
      handle.workspace ==
      workspace.string());

  const auto pidPath =
      workspace /
      "process.pid";

  const auto startTimePath =
      workspace /
      "process.start_time";

  const auto exitPath =
      workspace /
      "exit.code";

  const auto logPath =
      workspace /
      "bitcrack.log";

  const auto foundPath =
      workspace /
      "found.txt";

  assert(
      waitForFile(
          pidPath,
          std::chrono::seconds(2)));

  assert(
      waitForFile(
          startTimePath,
          std::chrono::seconds(2)));

  assert(
      waitForFile(
          exitPath,
          std::chrono::seconds(5)));

  assert(
      waitForFile(
          logPath,
          std::chrono::seconds(2)));

  assert(
      waitForFile(
          foundPath,
          std::chrono::seconds(2)));

  assertPrivateFile(
      pidPath);

  assertPrivateFile(
      startTimePath);

  assertPrivateFile(
      exitPath);

  assertPrivateFile(
      logPath);

  assertPrivateFile(
      foundPath);

  int storedPid = 0;

  {
    std::ifstream input(pidPath);
    input >> storedPid;
  }

  assert(storedPid == handle.pid);

  std::uint64_t storedStartTime = 0;

  {
    std::ifstream input(
        startTimePath);

    input >> storedStartTime;
  }

  assert(storedStartTime > 0);

  int exitCode = -9999;

  {
    std::ifstream input(exitPath);
    input >> exitCode;
  }

  assert(exitCode == 7);

  const auto log =
      readFile(logPath);

  assert(
      log.find("stdout-line") !=
      std::string::npos);

  assert(
      log.find("stderr-line") !=
      std::string::npos);

  std::filesystem::remove_all(
      workspace);

  std::cout
      << "BackgroundExecutionLauncherTests passed\n";

  return 0;
}
