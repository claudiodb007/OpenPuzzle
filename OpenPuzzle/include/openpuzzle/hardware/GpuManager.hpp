#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"

#include <vector>

namespace openpuzzle {

class GpuManager {
public:
  static std::vector<GpuInfo> listCudaGpus();
  static std::vector<GpuInfo> listOpenClGpus();

  static std::vector<GpuInfo> listGpus();      // compatibilidade
  static std::vector<GpuInfo> listAllGpus();   // novo

  static bool selectGpu(int device);
  static int selectedGpu();

  static GpuInfo currentGpu();
};

} // namespace openpuzzle
