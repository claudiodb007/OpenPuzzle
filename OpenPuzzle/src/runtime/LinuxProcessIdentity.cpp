#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include <unistd.h>
#include <sys/syscall.h>
#include <csignal>
#include <cerrno>

#include <fstream>
#include <sstream>
#include <string>

namespace openpuzzle {

std::optional<LinuxProcessIdentity::StartTime>
LinuxProcessIdentity::parseStartTime(
    const std::string& statLine) {
  /*
   * /proc/<pid>/stat begins:
   *
   *   1: pid
   *   2: comm enclosed in parentheses
   *   3: state
   *   ...
   *  22: starttime
   *
   * comm may contain spaces and parentheses, therefore tokenizing the
   * complete line on whitespace is unsafe. The final ')' terminates comm;
   * all fields after it are whitespace-delimited scalar values.
   */
  const auto close =
      statLine.rfind(')');

  if (close == std::string::npos ||
      close + 2 > statLine.size()) {
    return std::nullopt;
  }

  if (close + 1 >= statLine.size() ||
      statLine[close + 1] != ' ') {
    return std::nullopt;
  }

  std::istringstream fields(
      statLine.substr(close + 2));

  std::string value;

  /*
   * The first token after comm is field 3 (state).
   * starttime is field 22, therefore it is token 19
   * when counting from zero here.
   */
  for (int index = 0; index < 19; ++index) {
    if (!(fields >> value)) {
      return std::nullopt;
    }
  }

  StartTime startTime = 0;

  if (!(fields >> startTime)) {
    return std::nullopt;
  }

  return startTime;
}

std::optional<LinuxProcessIdentity::StartTime>
LinuxProcessIdentity::startTime(
    int pid) {
  if (pid <= 0) {
    return std::nullopt;
  }

  const std::string path =
      "/proc/" +
      std::to_string(pid) +
      "/stat";

  std::ifstream input(path);

  if (!input.is_open()) {
    return std::nullopt;
  }

  std::string line;

  if (!std::getline(input, line) ||
      line.empty()) {
    return std::nullopt;
  }

  return parseStartTime(line);
}


bool LinuxProcessIdentity::signalIfMatches(
    int pid,
    StartTime expectedStartTime,
    int signal) {
  if (pid <= 0 ||
      expectedStartTime == 0) {
    return false;
  }

#if defined(SYS_pidfd_open) && \
    defined(SYS_pidfd_send_signal)

  const int pidfd =
      static_cast<int>(
          syscall(
              SYS_pidfd_open,
              pid,
              0));

  if (pidfd < 0) {
    return false;
  }

  /*
   * The pidfd now pins the process object. Validate that the process
   * represented by the numeric PID is still the persisted instance.
   */
  const auto actualStartTime =
      startTime(pid);

  if (!actualStartTime ||
      *actualStartTime !=
          expectedStartTime) {
    close(pidfd);
    return false;
  }

  errno = 0;

  const long result =
      syscall(
          SYS_pidfd_send_signal,
          pidfd,
          signal,
          nullptr,
          0);

  const int savedErrno =
      errno;

  close(pidfd);
  errno = savedErrno;

  return result == 0;
#else
  /*
   * No unsafe kill(pid, signal) fallback.
   */
  (void)signal;
  return false;
#endif
}

} // namespace openpuzzle
