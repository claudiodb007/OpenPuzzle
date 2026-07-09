#pragma once

#include <string>

namespace openpuzzle {

class ExecutionStopper {
public:
  bool stop(const std::string& workspace) const;

private:
  static int readPid(const std::string& workspace);
};

} // namespace openpuzzle
