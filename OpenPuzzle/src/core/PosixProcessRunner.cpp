#include "openpuzzle/core/PosixProcessRunner.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace openpuzzle {

ProcessResult PosixProcessRunner::run(const std::string &command,
                                      const LineCallback &onLine,
                                      int maxSeconds) const {
  (void)maxSeconds;

  ProcessResult result;

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    return result;
  }

  pid_t pid = fork();

  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return result;
  }

  if (pid == 0) {
    close(pipefd[0]);

    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);

    close(pipefd[1]);

    execl("/bin/sh", "sh", "-c", command.c_str(), (char *)nullptr);

    _exit(127);
  }

  close(pipefd[1]);

  result.started = true;

  FILE *stream = fdopen(pipefd[0], "r");
  if (!stream) {
    close(pipefd[0]);
    return result;
  }

  std::array<char, 4096> buffer{};

  while (fgets(buffer.data(), buffer.size(), stream) != nullptr) {
    std::string line(buffer.data());

    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }

    if (onLine) {
      onLine(line);
    }
  }

  fclose(stream);

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else {
    result.exitCode = status;
  }

  return result;
}

} // namespace openpuzzle
