#pragma once

#include <string>

namespace openpuzzle {

struct ExecutionProgress {
  double speedMKeys = 0.0;

  std::string keysChecked;

  bool keyFound = false;
  std::string privateKey;

  bool finished = false;
  bool error = false;
  std::string message;
};

} // namespace openpuzzle
