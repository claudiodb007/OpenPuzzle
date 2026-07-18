#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class GpuManager {
public:
  static std::vector<GpuInfo> listCudaGpus();
  static std::vector<GpuInfo> listOpenClGpus();

  static std::vector<GpuInfo> listGpus();
  static std::vector<GpuInfo> listAllGpus();

  /*
   * Parse the backend-neutral output produced by
   * cuBitCrack/clBitCrack --list-devices.
   */
  static std::vector<GpuInfo>
  parseBitCrackDevices(
      const std::string &output,
      const std::string &backend);

  static bool selectGpu(int device);
  static int selectedGpu();

  static GpuInfo currentGpu();

  static GpuInfo currentGpu(
      const std::string &backend);
};

} // namespace openpuzzle
