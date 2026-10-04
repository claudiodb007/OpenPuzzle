#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ExecutionSlot.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#include <cerrno>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace openpuzzle {

std::filesystem::path
ClientRuntimeControl::pidPath() {
  return pidPath(
      client::ClientStateStore::
          executionSlot());
}

std::filesystem::path
ClientRuntimeControl::pidPath(
    const std::string& executionSlot) {
  const char *home =
      std::getenv("HOME");

  const auto root =
      home && *home != '\0'
          ? std::filesystem::path(home)
          : std::filesystem::current_path();

  const std::string filename =
      "runtime" +
      ExecutionSlot::fileSuffix(executionSlot) +
      ".pid";

  return root /
         ".local" /
         "share" /
         "OpenPuzzle" /
         filename;
}

std::filesystem::path
ClientRuntimeControl::safeStopPath() {
  return safeStopPath(
      client::ClientStateStore::
          executionSlot());
}

std::filesystem::path
ClientRuntimeControl::safeStopPath(
    const std::string& executionSlot) {
  const char *home =
      std::getenv("HOME");

  const auto root =
      home && *home != '\0'
          ? std::filesystem::path(home)
          : std::filesystem::current_path();

  const std::string filename =
      "safestop" +
      ExecutionSlot::fileSuffix(executionSlot) +
      ".requested";

  return root /
         ".local" /
         "share" /
         "OpenPuzzle" /
         filename;
}

std::optional<int>
ClientRuntimeControl::runtimePid() {
  return runtimePid(
      client::ClientStateStore::
          executionSlot());
}

std::optional<int>
ClientRuntimeControl::runtimePid(
    const std::string& executionSlot) {
  std::ifstream input(
      pidPath(executionSlot));

  if (!input.is_open()) {
    return std::nullopt;
  }

  int pid = 0;

  if (!(input >> pid) || pid <= 0) {
    return std::nullopt;
  }

  return pid;
}

std::optional<std::string>
ClientRuntimeControl::runtimeBootId() {
  return runtimeBootId(
      client::ClientStateStore::
          executionSlot());
}

std::optional<std::string>
ClientRuntimeControl::runtimeBootId(
    const std::string& executionSlot) {
  std::ifstream input(
      pidPath(executionSlot));

  if (!input.is_open()) {
    return std::nullopt;
  }

  int pid = 0;

  if (!(input >> pid) || pid <= 0) {
    return std::nullopt;
  }

  std::string bootId;

  if (!(input >> bootId) ||
      bootId.empty()) {
    return std::nullopt;
  }

  return bootId;
}

std::optional<std::uint64_t>
ClientRuntimeControl::runtimeStartTime() {
  return runtimeStartTime(
      client::ClientStateStore::
          executionSlot());
}

std::optional<std::uint64_t>
ClientRuntimeControl::runtimeStartTime(
    const std::string& executionSlot) {
  std::ifstream input(
      pidPath(executionSlot));

  if (!input.is_open()) {
    return std::nullopt;
  }

  int pid = 0;
  std::string bootId;
  std::uint64_t startTime = 0;

  if (!(input >> pid) ||
      pid <= 0 ||
      !(input >> bootId) ||
      bootId.empty() ||
      !(input >> startTime) ||
      startTime == 0) {
    return std::nullopt;
  }

  return startTime;
}

bool ClientRuntimeControl::running() {
  return running(
      client::ClientStateStore::
          executionSlot());
}

bool ClientRuntimeControl::running(
    const std::string& executionSlot) {
  return runtimeIdentityStatus(executionSlot) ==
      RuntimeIdentityStatus::Running;
}

ClientRuntimeControl::RuntimeIdentityStatus
ClientRuntimeControl::runtimeIdentityStatus(
    const std::string& executionSlot) {
  const auto path = pidPath(executionSlot);
  std::ifstream input(path);

  if (!input.is_open()) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    return exists || error
        ? RuntimeIdentityStatus::Unavailable
        : RuntimeIdentityStatus::Inactive;
  }

  int pid = 0;
  std::string bootId;
  std::uint64_t startTime = 0;
  if (!(input >> pid >> bootId >> startTime) ||
      pid <= 0 || bootId.empty() || startTime == 0) {
    // Incomplete legacy markers retain the established stale cleanup.
    return RuntimeIdentityStatus::Inactive;
  }

  const auto currentBootId =
      client::ClientStateStore::
          currentBootId();

  if (currentBootId.empty()) {
    return RuntimeIdentityStatus::Unavailable;
  }

  if (bootId != currentBootId) {
    return RuntimeIdentityStatus::Inactive;
  }

  if (kill(pid, 0) != 0) {
    if (errno == ESRCH) {
      return RuntimeIdentityStatus::Inactive;
    }
    if (errno != EPERM) {
      return RuntimeIdentityStatus::Unavailable;
    }
  }

  const auto currentStartTime =
      LinuxProcessIdentity::
          startTime(pid);

  if (!currentStartTime) {
    // A failed identity read does not prove exit. Recheck the read/exit race.
    return kill(pid, 0) != 0 && errno == ESRCH
        ? RuntimeIdentityStatus::Inactive
        : RuntimeIdentityStatus::Unavailable;
  }

  return *currentStartTime == startTime
      ? RuntimeIdentityStatus::Running
      : RuntimeIdentityStatus::Inactive;
}

bool ClientRuntimeControl::acquire() {
  const auto path =
      pidPath();

  std::error_code error;

  try {
    WorkspaceSecurity::prepare(
        path.parent_path());
  } catch (...) {
    return false;
  }

  const auto slot = client::ClientStateStore::executionSlot();
  if (runtimeIdentityStatus(slot) != RuntimeIdentityStatus::Inactive) {
    return false;
  }

  if (!clearSafeStop()) {
    return false;
  }

  for (int attempt = 0;
       attempt < 2;
       ++attempt) {
    const int descriptor =
        open(
            path.c_str(),
            O_WRONLY |
                O_CREAT |
                O_EXCL,
            0600);

    if (descriptor >= 0) {
      const auto bootId =
          client::ClientStateStore::
              currentBootId();

      const auto processStartTime =
          LinuxProcessIdentity::
              startTime(
                  static_cast<int>(
                      getpid()));

      if (bootId.empty() ||
          !processStartTime ||
          *processStartTime == 0) {
        close(descriptor);

        std::filesystem::remove(
            path,
            error);

        return false;
      }

      const std::string value =
          std::to_string(getpid()) +
          "\n" +
          bootId +
          "\n" +
          std::to_string(
              *processStartTime) +
          "\n";

      const auto written =
          write(
              descriptor,
              value.data(),
              value.size());

      const bool closed =
          close(descriptor) == 0;

      if (written !=
              static_cast<ssize_t>(
                  value.size()) ||
          !closed) {
        std::filesystem::remove(
            path,
            error);

        return false;
      }

      return true;
    }

    if (errno != EEXIST) {
      return false;
    }

    if (runtimeIdentityStatus(slot) != RuntimeIdentityStatus::Inactive) {
      return false;
    }

    /*
     * Only a confirmed stale identity or incomplete legacy marker may be
     * replaced. An unreadable identity still protects the existing runtime.
     */
    std::filesystem::remove(
        path,
        error);

    if (error) {
      return false;
    }
  }

  return false;
}

bool ClientRuntimeControl::release() {
  const auto existing =
      runtimePid();

  if (!existing) {
    return true;
  }

  /*
   * Nunca remover o controlo pertencente
   * a outro runtime ativo.
   */
  if (*existing !=
      static_cast<int>(getpid())) {
    return false;
  }

  const auto bootId =
      runtimeBootId();

  const auto currentBootId =
      client::ClientStateStore::
          currentBootId();

  const auto storedStartTime =
      runtimeStartTime();

  const auto currentStartTime =
      LinuxProcessIdentity::
          startTime(
              static_cast<int>(
                  getpid()));

  if (!bootId ||
      !storedStartTime ||
      !currentStartTime ||
      currentBootId.empty() ||
      *bootId != currentBootId ||
      *storedStartTime !=
          *currentStartTime) {
    return false;
  }

  std::error_code error;

  const bool removed =
      std::filesystem::remove(
          pidPath(),
          error);

  return !error &&
         (removed ||
          !std::filesystem::exists(
              pidPath()));
}

bool ClientRuntimeControl::requestStop() {
  return requestStop(
      client::ClientStateStore::
          executionSlot());
}

bool ClientRuntimeControl::requestStop(
    const std::string& executionSlot) {
  const auto pid = runtimePid(executionSlot);
  const auto bootId = runtimeBootId(executionSlot);
  const auto startTime = runtimeStartTime(executionSlot);

  const auto removeStaleMarker = [&]() {
    std::error_code error;
    std::filesystem::remove(pidPath(executionSlot), error);
    return false;
  };

  // Incomplete legacy markers cannot authorize a signal.
  if (!pid || !bootId || !startTime) {
    return removeStaleMarker();
  }

  const auto currentBootId =
      client::ClientStateStore::currentBootId();

  if (currentBootId.empty()) {
    return false;
  }

  if (*bootId != currentBootId) {
    return removeStaleMarker();
  }

  if (kill(*pid, 0) != 0) {
    if (errno == ESRCH) {
      return removeStaleMarker();
    }

    if (errno != EPERM) {
      return false;
    }
  }

  const auto currentStartTime =
      LinuxProcessIdentity::startTime(*pid);

  if (!currentStartTime) {
    // Unreadability does not prove exit. Check the read/exit race once more.
    if (kill(*pid, 0) != 0 && errno == ESRCH) {
      return removeStaleMarker();
    }
    return false;
  }

  if (*currentStartTime != *startTime) {
    return removeStaleMarker();
  }

  // Signalling revalidates through a pidfd. Permission, syscall availability
  // and other signal failures do not prove that this runtime has stopped.
  // Keep its control marker so retries and duplicate-launch protection work.
  return LinuxProcessIdentity::signalIfMatches(
      *pid, *startTime, SIGTERM);
}

bool ClientRuntimeControl::requestSafeStop() {
  return requestSafeStop(
      client::ClientStateStore::
          executionSlot());
}

bool ClientRuntimeControl::requestSafeStop(
    const std::string& executionSlot) {
  if (!running(executionSlot)) {
    return false;
  }

  const auto path =
      safeStopPath(executionSlot);

  try {
    WorkspaceSecurity::prepare(
        path.parent_path());
  } catch (...) {
    return false;
  }

  const int descriptor =
      open(
          path.c_str(),
          O_WRONLY |
              O_CREAT |
              O_TRUNC,
          0600);

  if (descriptor < 0) {
    return false;
  }

  const std::string value =
      "requested\n";

  const auto written =
      write(
          descriptor,
          value.data(),
          value.size());

  const bool closed =
      close(descriptor) == 0;

  if (written !=
          static_cast<ssize_t>(
              value.size()) ||
      !closed) {
    std::error_code error;
    std::filesystem::remove(
        path,
        error);
    return false;
  }

  if (!running(executionSlot)) {
    clearSafeStop(executionSlot);
    return false;
  }

  return true;
}

bool ClientRuntimeControl::safeStopRequested() {
  return safeStopRequested(
      client::ClientStateStore::
          executionSlot());
}

bool ClientRuntimeControl::safeStopRequested(
    const std::string& executionSlot) {
  std::error_code error;

  const bool exists =
      std::filesystem::exists(
          safeStopPath(executionSlot),
          error);

  return !error && exists;
}

bool ClientRuntimeControl::clearSafeStop() {
  return clearSafeStop(
      client::ClientStateStore::
          executionSlot());
}

bool ClientRuntimeControl::clearSafeStop(
    const std::string& executionSlot) {
  const auto path =
      safeStopPath(executionSlot);

  std::error_code error;

  const bool removed =
      std::filesystem::remove(
          path,
          error);

  return !error &&
         (removed ||
          !std::filesystem::exists(
              path));
}

} // namespace openpuzzle
