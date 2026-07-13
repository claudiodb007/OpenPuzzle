#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"

#include <cassert>
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

std::string readFile(
    const std::filesystem::path& path) {
  std::ifstream input(path);

  std::ostringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
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

} // namespace

int main() {
  const auto workspace =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-background-launcher-" +
          std::to_string(getpid())
      );

  std::filesystem::remove_all(
      workspace);

  StartExecutionRequest request;

  request.executionId = 77;

  request.workspace =
      workspace.string();

  request.command =
      "echo stdout-line; "
      "echo stderr-line >&2; "
      "exit 7";

  BackgroundExecutionLauncher launcher;

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

  const auto exitPath =
      workspace /
      "exit.code";

  const auto logPath =
      workspace /
      "bitcrack.log";

  assert(
      waitForFile(
          pidPath,
          std::chrono::seconds(2)));

  assert(
      waitForFile(
          exitPath,
          std::chrono::seconds(5)));

  assert(
      waitForFile(
          logPath,
          std::chrono::seconds(2)));

  int storedPid = 0;

  {
    std::ifstream input(pidPath);
    input >> storedPid;
  }

  assert(storedPid == handle.pid);

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
