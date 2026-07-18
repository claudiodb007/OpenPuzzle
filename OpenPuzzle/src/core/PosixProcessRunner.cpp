#include "openpuzzle/core/PosixProcessRunner.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace openpuzzle {

namespace {

void signalProcessGroup(
    pid_t pid,
    int signal) {
  if (pid <= 0) {
    return;
  }

  kill(-pid, signal);
  kill(pid, signal);
}

bool emitRecords(
    std::string &pending,
    const IProcessRunner::LineCallback &onLine,
    const IProcessRunner::StopPredicate &stop) {
  while (true) {
    const auto position =
        pending.find_first_of("\r\n");

    if (position == std::string::npos) {
      return false;
    }

    std::string line =
        pending.substr(
            0,
            position);

    std::size_t consumed =
        position + 1;

    while (
        consumed < pending.size() &&
        (
            pending[consumed] == '\r' ||
            pending[consumed] == '\n'
        )) {
      ++consumed;
    }

    pending.erase(
        0,
        consumed);

    if (!line.empty() &&
        onLine) {
      onLine(line);
    }

    if (stop &&
        stop()) {
      return true;
    }
  }
}

} // namespace

ProcessResult PosixProcessRunner::run(
    const std::string &command,
    const LineCallback &onLine,
    int maxSeconds,
    const StopPredicate &stop) const {
  ProcessResult result;

  int pipefd[2];

  if (pipe(pipefd) == -1) {
    return result;
  }

  const pid_t pid =
      fork();

  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);

    return result;
  }

  if (pid == 0) {
    close(pipefd[0]);

    /*
     * Place the shell and its engine process in a
     * dedicated group so timeout termination cannot
     * leave a GPU process behind.
     */
    setpgid(0, 0);

    dup2(
        pipefd[1],
        STDOUT_FILENO);

    dup2(
        pipefd[1],
        STDERR_FILENO);

    close(pipefd[1]);

    execl(
        "/bin/sh",
        "sh",
        "-c",
        command.c_str(),
        static_cast<char *>(nullptr));

    _exit(127);
  }

  close(pipefd[1]);

  setpgid(
      pid,
      pid);

  result.started = true;

  const int currentFlags =
      fcntl(
          pipefd[0],
          F_GETFL,
          0);

  if (currentFlags >= 0) {
    fcntl(
        pipefd[0],
        F_SETFL,
        currentFlags | O_NONBLOCK);
  }

  const auto startedAt =
      std::chrono::steady_clock::now();

  std::string pending;

  bool interrupted = false;
  bool pipeClosed = false;
  bool childWaited = false;

  int status = 0;

  while (!pipeClosed) {
    if (maxSeconds > 0) {
      const auto elapsed =
          std::chrono::duration_cast<
              std::chrono::seconds>(
              std::chrono::steady_clock::now() -
              startedAt);

      if (elapsed.count() >=
          maxSeconds) {
        interrupted = true;
        break;
      }
    }

    if (stop &&
        stop()) {
      interrupted = true;
      break;
    }

    pollfd descriptor{};
    descriptor.fd = pipefd[0];
    descriptor.events =
        POLLIN |
        POLLHUP |
        POLLERR;

    const int pollResult =
        poll(
            &descriptor,
            1,
            100);

    if (pollResult < 0) {
      if (errno == EINTR) {
        continue;
      }

      break;
    }

    if (
        pollResult > 0 &&
        (
            descriptor.revents &
            (
                POLLIN |
                POLLHUP |
                POLLERR
            )
        )) {
      char buffer[4096];

      while (true) {
        const ssize_t count =
            read(
                pipefd[0],
                buffer,
                sizeof(buffer));

        if (count > 0) {
          pending.append(
              buffer,
              static_cast<std::size_t>(
                  count));

          if (emitRecords(
                  pending,
                  onLine,
                  stop)) {
            interrupted = true;
            break;
          }

          continue;
        }

        if (count == 0) {
          pipeClosed = true;
          break;
        }

        if (errno == EINTR) {
          continue;
        }

        if (
            errno == EAGAIN ||
            errno == EWOULDBLOCK) {
          break;
        }

        pipeClosed = true;
        break;
      }
    }

    if (interrupted) {
      break;
    }

    const pid_t waitResult =
        waitpid(
            pid,
            &status,
            WNOHANG);

    if (waitResult == pid) {
      childWaited = true;

      /*
       * Continue until the pipe reaches EOF so all
       * buffered output is delivered.
       */
    }
  }

  if (interrupted) {
    signalProcessGroup(
        pid,
        SIGTERM);

    const auto terminationDeadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(1);

    while (!childWaited &&
           std::chrono::steady_clock::now() <
               terminationDeadline) {
      const pid_t waitResult =
          waitpid(
              pid,
              &status,
              WNOHANG);

      if (waitResult == pid) {
        childWaited = true;
        break;
      }

      std::this_thread::sleep_for(
          std::chrono::milliseconds(25));
    }

    if (!childWaited) {
      signalProcessGroup(
          pid,
          SIGKILL);

      waitpid(
          pid,
          &status,
          0);

      childWaited = true;
    }
  }

  close(pipefd[0]);

  if (!pending.empty()) {
    if (onLine) {
      onLine(pending);
    }

    pending.clear();
  }

  if (!childWaited) {
    waitpid(
        pid,
        &status,
        0);
  }

  if (interrupted) {
    result.exitCode = 124;
  } else if (WIFEXITED(status)) {
    result.exitCode =
        WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exitCode =
        128 +
        WTERMSIG(status);
  } else {
    result.exitCode = status;
  }

  return result;
}

} // namespace openpuzzle
