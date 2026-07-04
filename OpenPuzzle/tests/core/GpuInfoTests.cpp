#include "openpuzzle/hardware/GpuInfo.hpp"

using namespace openpuzzle;

int main() {
  GpuInfo gpu;

  if (gpu.id != 0)
    return 1;
  if (!gpu.name.empty())
    return 2;
  if (!gpu.backend.empty())
    return 3;
  if (gpu.memoryMb != 0)
    return 4;
  if (gpu.computeUnits != 0)
    return 5;

  return 0;
}
