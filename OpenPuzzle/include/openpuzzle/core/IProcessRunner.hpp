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
  using StopPredicate = std::function<bool()>;

  virtual ~IProcessRunner() = default;

  virtual ProcessResult run(const std::string &command,
                            const LineCallback &onLine, int maxSeconds = 0,
                            const StopPredicate &stop = nullptr) const = 0;
};

} // namespace openpuzzle
