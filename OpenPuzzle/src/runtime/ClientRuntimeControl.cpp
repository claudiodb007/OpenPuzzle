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

bool ClientRuntimeControl::processExists(
    int pid) {
  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
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
  const auto pid =
      runtimePid(executionSlot);

  const auto bootId =
      runtimeBootId(executionSlot);

  const auto startTime =
      runtimeStartTime(
          executionSlot);

  const auto currentBootId =
      client::ClientStateStore::
          currentBootId();

  if (!pid ||
      !bootId ||
      !startTime ||
      currentBootId.empty() ||
      *bootId != currentBootId ||
      !processExists(*pid)) {
    return false;
  }

  const auto currentStartTime =
      LinuxProcessIdentity::
          startTime(*pid);

  return
      currentStartTime &&
      *currentStartTime ==
          *startTime;
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

  if (running()) {
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

    if (running()) {
      return false;
    }

    /*
     * Marcador inválido, pertencente a outro boot
     * ou processo já terminado.
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
  const auto pid =
      runtimePid(executionSlot);

  const auto bootId =
      runtimeBootId(executionSlot);

  const auto startTime =
      runtimeStartTime(executionSlot);

  const auto currentBootId =
      client::ClientStateStore::
          currentBootId();

  if (!pid ||
      !bootId ||
      !startTime ||
      currentBootId.empty() ||
      *bootId != currentBootId) {
    std::error_code error;

    std::filesystem::remove(
        pidPath(executionSlot),
        error);

    return false;
  }

  if (!LinuxProcessIdentity::
          signalIfMatches(
              *pid,
              *startTime,
              SIGTERM)) {
    /*
     * Process disappeared, PID identity changed, or pidfd signalling is
     * unavailable. Fail closed and never signal by numeric PID alone.
     */
    std::error_code error;

    std::filesystem::remove(
        pidPath(executionSlot),
        error);

    return false;
  }

  return true;
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
