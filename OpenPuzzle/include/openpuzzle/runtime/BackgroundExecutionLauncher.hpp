#pragma once

#include "openpuzzle/runtime/ExecutionHandle.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

namespace openpuzzle {

class BackgroundExecutionLauncher {
public:
  ExecutionHandle start(const StartExecutionRequest& request) const;
};

} // namespace openpuzzle
