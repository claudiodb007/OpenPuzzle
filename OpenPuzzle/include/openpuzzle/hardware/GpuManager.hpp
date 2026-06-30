#pragma once
#include <string>
#include <vector>
namespace openpuzzle {
struct GpuInfo {
  int device = 0;
  std::string name;
  std::string uuid;
  std::string memory;
};
class GpuManager {
public:
  static std::vector<GpuInfo> listGpus();
  static bool selectGpu(int device);
  static int selectedGpu();
};
} // namespace openpuzzle
