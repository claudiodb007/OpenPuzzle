#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

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
  const char *home =
      std::getenv("HOME");

  const auto root =
      home && *home != '\0'
          ? std::filesystem::path(home)
          : std::filesystem::current_path();

  return root /
         ".local" /
         "share" /
         "OpenPuzzle" /
         "runtime.pid";
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
  std::ifstream input(
      pidPath());

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
  const auto pid =
      runtimePid();

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
  const auto pid =
      runtimePid();

  if (!pid) {
    return false;
  }

  if (!processExists(*pid)) {
    std::error_code error;

    std::filesystem::remove(
        pidPath(),
        error);

    return false;
  }

  return kill(*pid, SIGTERM) == 0;
}

} // namespace openpuzzle
