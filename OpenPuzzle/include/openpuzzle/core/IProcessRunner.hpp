#pragma once

#include <functional>
#include <string>

namespace openpuzzle {

struct ProcessResult {
  int exitCode = -1;
  bool started = false;
};

class IProcessRunner {
public:
  using LineCallback = std::function<void(const std::string &)>;

  virtual ~IProcessRunner() = default;

  virtual ProcessResult run(const std::string &command,
                            const LineCallback &onLine,
                            int maxSeconds = 0) const = 0;
};

} // namespace openpuzzle
