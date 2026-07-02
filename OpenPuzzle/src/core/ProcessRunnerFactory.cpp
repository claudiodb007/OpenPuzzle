#include "openpuzzle/core/ProcessRunnerFactory.hpp"

#include "openpuzzle/core/PosixProcessRunner.hpp"

namespace openpuzzle {

std::unique_ptr<IProcessRunner> ProcessRunnerFactory::create() {
  return std::make_unique<PosixProcessRunner>();
}

} // namespace openpuzzle
