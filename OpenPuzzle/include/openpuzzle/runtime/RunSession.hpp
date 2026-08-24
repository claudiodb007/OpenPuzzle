#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"
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

  static std::vector<std::string>
  concurrentCudaArguments(
      const std::vector<std::string> &args);

  static std::vector<std::string>
  concurrentOpenclArguments(
      const std::vector<std::string> &args);

  static std::vector<std::string>
  concurrentPreflightArguments(
      const std::vector<std::string> &args);

  static void validateConcurrentGpuSelection(
      const std::vector<std::string> &cudaArguments,
      const std::vector<std::string> &openclArguments,
      const std::vector<GpuInfo> &cudaDevices,
      const std::vector<GpuInfo> &openclDevices);

private:
  ClientIterationResult runOnce(
      const std::vector<std::string> &args,
      bool initializeClient = true) const;
};

} // namespace openpuzzle
