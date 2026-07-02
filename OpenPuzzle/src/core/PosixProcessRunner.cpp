#include "openpuzzle/core/PosixProcessRunner.hpp"

#include "openpuzzle/core/PopenProcessRunner.hpp"

namespace openpuzzle {

ProcessResult PosixProcessRunner::run(const std::string &command,
                                      const LineCallback &onLine,
                                      int maxSeconds) const {
  PopenProcessRunner fallback;
  return fallback.run(command, onLine, maxSeconds);
}

} // namespace openpuzzle
