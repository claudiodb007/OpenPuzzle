#pragma once

#include <string>

namespace openpuzzle {

struct ExecutionHandle {
  int executionId = 0;
  int pid = 0;
  std::string workspace;
};

} // namespace openpuzzle
