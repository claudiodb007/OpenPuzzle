#pragma once

#include "openpuzzle/core/IProcessRunner.hpp"

#include <memory>

namespace openpuzzle {

class ProcessRunnerFactory {
public:
  static std::unique_ptr<IProcessRunner> create();
};

} // namespace openpuzzle
