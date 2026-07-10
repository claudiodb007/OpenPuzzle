#include "openpuzzle/hardware/GpuInfo.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

int main() {
  GpuInfo gpu;

  gpu.device = 1;
  gpu.name = "RTX 4070 Super";
  gpu.backend = "CUDA";
  gpu.uuid = "GPU-test";
  gpu.driverVersion = "580.00";
  gpu.memoryMb = 12288;
  gpu.computeUnits = 56;
  gpu.cuda = true;
  gpu.opencl = false;

  assert(gpu.device == 1);
  assert(gpu.name == "RTX 4070 Super");
  assert(gpu.backend == "CUDA");
  assert(gpu.uuid == "GPU-test");
  assert(gpu.driverVersion == "580.00");
  assert(gpu.memoryMb == 12288);
  assert(gpu.computeUnits == 56);
  assert(gpu.cuda);
  assert(!gpu.opencl);

  std::cout << "GpuInfoTests passed\n";
  return 0;
}
