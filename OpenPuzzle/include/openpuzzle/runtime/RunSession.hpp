#pragma once

#include "openpuzzle/runtime/ClientRuntime.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class RunSession {
public:
  int run(
      const std::vector<std::string> &args) const;

  static std::vector<std::string>
  concurrentGpuArguments(
      const std::vector<std::string> &args);

  static std::vector<std::string>
  concurrentCpuArguments(
      const std::vector<std::string> &args);

private:
  ClientIterationResult runOnce(
      const std::vector<std::string> &args,
      bool initializeClient = true) const;
};

} // namespace openpuzzle
