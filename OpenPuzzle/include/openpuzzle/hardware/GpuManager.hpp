#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"

#include <vector>

namespace openpuzzle {

class GpuManager {
public:
  static std::vector<GpuInfo> listGpus();

  static bool selectGpu(int device);
  static int selectedGpu();
  static GpuInfo currentGpu();
};

} // namespace openpuzzle
