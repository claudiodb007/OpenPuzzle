#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace openpuzzle {

ExecutionHandle BackgroundExecutionLauncher::start(
    const StartExecutionRequest& request) const {
  if (request.command.empty()) {
    throw std::runtime_error("Cannot start empty command");
  }

  if (!request.workspace.empty()) {
    std::filesystem::create_directories(request.workspace);
  }

  auto workspacePath = std::filesystem::path(request.workspace);

  auto pidFile = (workspacePath / "process.pid").string();
  auto exitFile = (workspacePath / "exit.code").string();

  std::ostringstream shell;
  shell << "setsid sh -c '("
        << request.command
        << "); rc=$?; echo $rc > "
        << exitFile
        << "; exit $rc"
        << "' >/dev/null 2>&1 & echo $!";

  FILE* pipe = popen(shell.str().c_str(), "r");

  if (!pipe) {
    throw std::runtime_error("Failed to start background process");
  }

  char buffer[128] = {0};
  std::string output;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  int rc = pclose(pipe);

  if (rc == -1 || output.empty()) {
    throw std::runtime_error("Failed to read background process pid");
  }

  int pid = std::stoi(output);

  std::ofstream out(pidFile);
  if (out.is_open()) {
    out << pid << "\n";
  }

  ExecutionHandle handle;
  handle.executionId = request.executionId;
  handle.pid = pid;
  handle.workspace = request.workspace;

  return handle;
}

} // namespace openpuzzle
