#pragma once

#include "openpuzzle/core/ExecutionResult.hpp"

#include <functional>
#include <string>

namespace openpuzzle {

struct ProcessContext {

  std::string command;

  std::string workspace;

  bool echoOutput = true;

  std::function<void(const ExecutionResult&)> onProgress;

};

} // namespace openpuzzle
