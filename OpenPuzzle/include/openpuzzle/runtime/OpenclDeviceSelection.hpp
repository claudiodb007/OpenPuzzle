#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class OpenclDeviceSelection {
public:
  /*
   * Resolves either "all" or a comma-separated device list against the
   * OpenCL inventory. The result is deterministic and contains no duplicates.
   */
  static std::vector<int> resolve(
      const std::string& selector,
      const std::vector<GpuInfo>& availableDevices);
};

} // namespace openpuzzle
