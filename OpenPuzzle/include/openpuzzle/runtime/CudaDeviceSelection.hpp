#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class CudaDeviceSelection {
public:
  /*
   * Resolves either "all" or a comma-separated device list against the
   * CUDA inventory. The result is deterministic and contains no duplicates.
   */
  static std::vector<int> resolve(
      const std::string& selector,
      const std::vector<GpuInfo>& availableDevices);
};

} // namespace openpuzzle
