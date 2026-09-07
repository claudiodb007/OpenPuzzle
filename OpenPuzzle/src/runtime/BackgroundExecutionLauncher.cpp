#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"

#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace openpuzzle {
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

bool workspaceExecutionIsActive(
    const std::filesystem::path& workspace) {
  std::ifstream pidInput(
      workspace / "process.pid");

  std::ifstream bootInput(
      workspace / "process.boot_id");

  std::ifstream startTimeInput(
      workspace / "process.start_time");

  int pid = 0;
  std::string recordedBootId;
  std::uint64_t recordedStartTime = 0;

  if (!(pidInput >> pid) ||
      !(bootInput >> recordedBootId) ||
      !(startTimeInput >> recordedStartTime) ||
      pid <= 0 ||
      recordedBootId.empty() ||
      recordedStartTime == 0) {
    return false;
  }

  const auto currentBootId =
      client::ClientStateStore::currentBootId();

  if (currentBootId.empty() ||
      recordedBootId != currentBootId) {
    return false;
  }

  errno = 0;

  if (kill(pid, 0) != 0 &&
      errno != EPERM) {
    return false;
  }

  const auto currentStartTime =
      LinuxProcessIdentity::startTime(pid);

  return currentStartTime &&
         *currentStartTime ==
             recordedStartTime;
}

} // namespace

ExecutionHandle BackgroundExecutionLauncher::start(
    const StartExecutionRequest& request) const {
  if (request.command.empty()) {
    throw std::runtime_error("Cannot start empty command");
  }

  if (!request.workspace.empty()) {
    WorkspaceSecurity::prepare(
        request.workspace);
  }

  auto workspacePath = std::filesystem::path(request.workspace);

  /*
   * Última barreira contra relançamentos duplicados.
   * Mesmo que o estado principal seja lido incorretamente,
   * nunca substituir a identidade de um processo ainda vivo
   * no mesmo workspace.
   */
  if (!request.workspace.empty() &&
      workspaceExecutionIsActive(
          workspacePath)) {
    throw std::runtime_error(
        "An execution is already active in this workspace");
  }

  auto pidFile = (workspacePath / "process.pid").string();
  auto bootIdFile =
      (workspacePath / "process.boot_id").string();

  auto startTimeFile =
      (workspacePath / "process.start_time").string();

  auto exitFile = (workspacePath / "exit.code").string();
  auto logFile = (workspacePath / "bitcrack.log").string();

  /*
   * Determine system identity before starting the supervisor.
   *
   * If boot identity cannot be established, no process may be launched:
   * otherwise we could create an execution that cannot later be identified
   * safely.
   */
  const auto bootId =
      client::ClientStateStore::currentBootId();

  if (bootId.empty()) {
    throw std::runtime_error(
        "Failed to determine process boot identity");
  }

  std::ostringstream supervisor;
  supervisor
      << '(' << request.command
      << "); rc=$?; printf '%s\\n' \"$rc\" > "
      << shellQuote(exitFile)
      << "; exit \"$rc\"";

  std::ostringstream session;
  session
      << "printf '%s\\n' \"$$\" >&3; "
      << "exec 3>&-; "
      << supervisor.str();

  std::ostringstream shell;
  shell
      << "umask 077; setsid sh -c "
      << shellQuote(session.str())
      << " 3>&1"
      << " >> "
      << shellQuote(logFile)
      << " 2>&1 &";

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

  /*
   * Capture the identity of this exact process instance immediately
   * after launch. If the process has already disappeared, do not create
   * an incomplete identity marker.
   */
  const auto processStartTime =
      LinuxProcessIdentity::startTime(pid);

  if (!processStartTime ||
      *processStartTime == 0) {
    throw std::runtime_error(
        "Failed to determine process start identity");
  }

  std::ofstream out(pidFile);

  if (!out.is_open()) {
    throw std::runtime_error(
        "Failed to create process pid file");
  }

  out << pid << "\n";
  out.close();

  if (!out) {
    throw std::runtime_error(
        "Failed to write process pid file");
  }

  WorkspaceSecurity::protectFile(
      pidFile);

  std::ofstream bootOut(
      bootIdFile);

  if (!bootOut.is_open()) {
    throw std::runtime_error(
        "Failed to create process boot id file");
  }

  bootOut << bootId << "\n";
  bootOut.close();

  if (!bootOut) {
    throw std::runtime_error(
        "Failed to write process boot id file");
  }

  WorkspaceSecurity::protectFile(
      bootIdFile);

  std::ofstream startTimeOut(
      startTimeFile);

  if (!startTimeOut.is_open()) {
    throw std::runtime_error(
        "Failed to create process start time file");
  }

  startTimeOut
      << *processStartTime
      << "\n";

  startTimeOut.close();

  if (!startTimeOut) {
    throw std::runtime_error(
        "Failed to write process start time file");
  }

  WorkspaceSecurity::protectFile(
      startTimeFile);

  ExecutionHandle handle;
  handle.executionId = request.executionId;
  handle.pid = pid;
  handle.workspace = request.workspace;

  return handle;
}

} // namespace openpuzzle
