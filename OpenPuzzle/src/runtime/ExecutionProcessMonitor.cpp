#include "openpuzzle/runtime/ExecutionProcessMonitor.hpp"

#include "openpuzzle/core/ExecutionRecord.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"

#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>

namespace openpuzzle {

ExecutionProcessMonitor::ExecutionProcessMonitor(Database& database)
    : database_(database) {}

int ExecutionProcessMonitor::readPid(const std::string& workspace) {
  auto pidFile = std::filesystem::path(workspace) / "process.pid";

  std::ifstream in(pidFile);

  if (!in.is_open()) {
    return 0;
  }

  int pid = 0;
  in >> pid;

  return pid;
}

int ExecutionProcessMonitor::readExitCode(const std::string& workspace) {
  auto exitFile = std::filesystem::path(workspace) / "exit.code";

  std::ifstream in(exitFile);

  if (!in.is_open()) {
    return -1;
  }

  int code = -1;
  in >> code;

  return code;
}

static std::string readProcessBootId(
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

static std::optional<
    LinuxProcessIdentity::StartTime>
readProcessStartTime(
    const std::string& workspace) {
  const auto path =
      std::filesystem::path(workspace) /
      "process.start_time";

  std::ifstream input(path);

  LinuxProcessIdentity::StartTime value = 0;

  if (!input.is_open() ||
      !(input >> value) ||
      value == 0) {
    return std::nullopt;
  }

  return value;
}

static bool processIdentityMatches(
    const std::string& workspace,
    int pid) {
  const auto storedBootId =
      readProcessBootId(workspace);

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

  const auto storedStartTime =
      readProcessStartTime(workspace);

  if (!storedStartTime) {
    return false;
  }

  if (kill(pid, 0) != 0 &&
      errno != EPERM) {
    return false;
  }

  const auto currentStartTime =
      LinuxProcessIdentity::
          startTime(pid);

  return
      currentStartTime &&
      *currentStartTime ==
          *storedStartTime;
}

bool ExecutionProcessMonitor::processExists(int pid) {
  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
}

ExecutionProcessMonitorSummary ExecutionProcessMonitor::poll() {
  ExecutionProcessMonitorSummary summary;

  auto executions = database_.listRunningExecutions();

  for (const auto& execution : executions) {
    if (execution.status != ExecutionRecordStatus::Running) {
      continue;
    }

    ++summary.running;

    int pid = readPid(execution.workspace);

    if (pid <= 0) {
      ++summary.missingPid;
      database_.finishExecution(execution.executionId, "failed", -1);
      ++summary.failed;
      continue;
    }

    if (processIdentityMatches(
            execution.workspace,
            pid)) {
      continue;
    }

    int exitCode = readExitCode(execution.workspace);

    if (exitCode == 0) {
      database_.finishExecution(execution.executionId, "finished", exitCode);
      ++summary.finished;
    } else if (exitCode == -2) {
      database_.finishExecution(execution.executionId, "cancelled", exitCode);
      ++summary.cancelled;
    } else {
      database_.finishExecution(execution.executionId, "failed", exitCode);
      ++summary.failed;
    }
  }

  return summary;
}

} // namespace openpuzzle
