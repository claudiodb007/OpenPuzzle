#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#include <cerrno>
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

  std::string filename =
      "runtime.pid";

  if (executionSlot == "gpu") {
    filename = "runtime-gpu.pid";
  } else if (executionSlot == "cpu") {
    filename = "runtime-cpu.pid";
  }

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

bool ClientRuntimeControl::running() {
  return running(
      client::ClientStateStore::
          executionSlot());
}

bool ClientRuntimeControl::running(
    const std::string& executionSlot) {
  const auto pid =
      runtimePid(executionSlot);

  return pid &&
         processExists(*pid);
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
      const std::string value =
          std::to_string(getpid()) +
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

    const auto existing =
        runtimePid();

    if (existing &&
        processExists(*existing)) {
      return false;
    }

    /*
     * PID inválido ou processo já terminado.
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

  if (!pid) {
    return false;
  }

  if (!processExists(*pid)) {
    std::error_code error;

    std::filesystem::remove(
        pidPath(executionSlot),
        error);

    return false;
  }

  return kill(*pid, SIGTERM) == 0;
}

} // namespace openpuzzle
