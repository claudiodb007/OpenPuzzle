#include "openpuzzle/core/ProcessRunner.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <sys/wait.h>

namespace openpuzzle {

ProcessResult ProcessRunner::run(const std::string &command,
                                 const LineCallback &onLine) const {
  ProcessResult result;
  FILE *pipe = popen(command.c_str(), "r");

  if (!pipe) {
    return result;
  }

  result.started = true;

  std::array<char, 4096> buffer{};

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
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
