#include "openpuzzle/core/ProcessRunnerFactory.hpp"

#ifdef _WIN32
#include "openpuzzle/core/WindowsProcessRunner.hpp"
#else
#include "openpuzzle/core/PosixProcessRunner.hpp"
#endif

namespace openpuzzle {

std::unique_ptr<IProcessRunner> ProcessRunnerFactory::create() {
#ifdef _WIN32
  return std::make_unique<WindowsProcessRunner>();
#else
  return std::make_unique<PosixProcessRunner>();
#endif
}

} // namespace openpuzzle
