#include "openpuzzle/runtime/ExecutionResourceFactory.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace openpuzzle;

int main() {
  GpuInfo cudaGpu;
  cudaGpu.device = 0;
  cudaGpu.name = "RTX 4070 Super";
  cudaGpu.backend = "CUDA";
  cudaGpu.uuid = "GPU-CUDA-0";
  cudaGpu.memoryMb = 12288;
  cudaGpu.cuda = true;

  GpuInfo openclGpu;
  openclGpu.device = 1;
  openclGpu.name = "RX 5700 XT";
  openclGpu.backend = "OpenCL";
  openclGpu.uuid = "GPU-OPENCL-1";
  openclGpu.memoryMb = 8192;
  openclGpu.opencl = true;

  auto cuda =
      ExecutionResourceFactory::fromGpu(
          cudaGpu,
          "BitCrack");

  assert(cuda.id == "GPU-CUDA-0:BitCrack");
  assert(cuda.name == "RTX 4070 Super");
  assert(cuda.engine == "BitCrack");
  assert(cuda.backend == "CUDA");
  assert(cuda.device == 0);
  assert(cuda.memoryMb == 12288);
  assert(cuda.available);

  assert(cuda.capability.engine == "BitCrack");
  assert(cuda.capability.backend == "CUDA");
  assert(cuda.capability.device == 0);
  assert(cuda.capability.vramMb == 12288);

  assert(cuda.matches("BitCrack", "CUDA"));
  assert(!cuda.matches("BitCrack", "OpenCL"));
  assert(!cuda.matches("KeyHunt", "CUDA"));

  std::vector<GpuInfo> gpus{
      cudaGpu,
      openclGpu
  };

  auto resources =
      ExecutionResourceFactory::fromGpus(
          gpus,
          "BitCrack");

  assert(resources.size() == 2);

  assert(resources[0].backend == "CUDA");
  assert(resources[0].device == 0);

  assert(resources[1].backend == "OpenCL");
  assert(resources[1].device == 1);
  assert(resources[1].memoryMb == 8192);

  std::cout
      << "ExecutionResourceFactoryTests passed\n";

  return 0;
}
