#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"
#include "openpuzzle/runtime/ExecutionResource.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class ExecutionResourceFactory {
public:
  static ExecutionResource fromGpu(
      const GpuInfo& gpu,
      const std::string& engine);

  static std::vector<ExecutionResource> fromGpus(
      const std::vector<GpuInfo>& gpus,
      const std::string& engine);
};

} // namespace openpuzzle
