#include "openpuzzle/core/PopenProcessRunner.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <sys/wait.h>

namespace openpuzzle {

ProcessResult PopenProcessRunner::run(const std::string &command,
                                      const LineCallback &onLine,
                                      int maxSeconds) const {
  ProcessResult result;
  FILE *pipe = popen(command.c_str(), "r");

  if (!pipe) {
    return result;
  }

  result.started = true;

  std::array<char, 4096> buffer{};
  const auto startedAt = std::chrono::steady_clock::now();

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    if (maxSeconds > 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - startedAt);

      if (elapsed.count() >= maxSeconds) {
        break;
      }
    }
    std::string line(buffer.data());

    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }

    if (onLine) {
      onLine(line);
    }
  }

  int status = pclose(pipe);

  if (status == -1) {
    result.exitCode = -1;
  } else if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else {
    result.exitCode = status;
  }

  return result;
}

} // namespace openpuzzle
