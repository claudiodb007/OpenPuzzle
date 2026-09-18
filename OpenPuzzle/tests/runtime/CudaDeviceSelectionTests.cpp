#include "openpuzzle/runtime/CudaDeviceSelection.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using openpuzzle::CudaDeviceSelection;
using openpuzzle::GpuInfo;

namespace {

GpuInfo cudaGpu(
    const int device,
    const std::string& name) {
  GpuInfo gpu;
  gpu.device = device;
  gpu.name = name;
  gpu.backend = "CUDA";
  gpu.memoryMb = 10240;
  gpu.cuda = true;
  return gpu;
}

void expectFailure(
    const std::string& selector,
    const std::vector<GpuInfo>& available,
    const std::string& message) {
  try {
    (void) CudaDeviceSelection::resolve(
        selector,
        available);
  } catch (const std::runtime_error& error) {
    assert(
        std::string(error.what()).find(message) !=
        std::string::npos);
    return;
  }

  assert(false);
}

} // namespace

int main() {
  const std::vector<GpuInfo> sixGpus = {
      cudaGpu(5, "GPU 5"),
      cudaGpu(2, "GPU 2"),
      cudaGpu(0, "GPU 0"),
      cudaGpu(4, "GPU 4"),
      cudaGpu(1, "GPU 1"),
      cudaGpu(3, "GPU 3"),
  };

  assert(
      CudaDeviceSelection::resolve(
          "all",
          sixGpus) ==
      std::vector<int>({0, 1, 2, 3, 4, 5}));

  assert(
      CudaDeviceSelection::resolve(
          "5,3,1",
          sixGpus) ==
      std::vector<int>({5, 3, 1}));

  assert(
      CudaDeviceSelection::resolve(
          " 0, 2, 4 ",
          sixGpus) ==
      std::vector<int>({0, 2, 4}));

  std::vector<GpuInfo> manyGpus;
  for (int device = 0; device < 128; ++device) {
    manyGpus.push_back(
        cudaGpu(
            device,
            "GPU " + std::to_string(device)));
  }

  const auto all =
      CudaDeviceSelection::resolve(
          "all",
          manyGpus);

  assert(all.size() == 128);
  assert(all.front() == 0);
  assert(all.back() == 127);

  expectFailure(
      "",
      sixGpus,
      "--devices requires");
  expectFailure(
      "ALL",
      sixGpus,
      "whole numbers");
  expectFailure(
      "0,",
      sixGpus,
      "empty item");
  expectFailure(
      ",0",
      sixGpus,
      "empty item");
  expectFailure(
      "0,,1",
      sixGpus,
      "empty item");
  expectFailure(
      "0,0",
      sixGpus,
      "selected more than once");
  expectFailure(
      "-1",
      sixGpus,
      "non-negative whole numbers");
  expectFailure(
      "1x",
      sixGpus,
      "non-negative whole numbers");
  expectFailure(
      "2147483648",
      sixGpus,
      "non-negative whole numbers");
  expectFailure(
      "0,9",
      sixGpus,
      "device 9 was not found");
  expectFailure(
      "all",
      {},
      "No CUDA devices");
  expectFailure(
      "all",
      {
          cudaGpu(0, "GPU A"),
          cudaGpu(0, "GPU B"),
      },
      "duplicate device 0");

  auto invalidInventory = sixGpus;
  invalidInventory.push_back(
      cudaGpu(-1, "Invalid"));
  expectFailure(
      "all",
      invalidInventory,
      "invalid device index");

  std::cout
      << "CudaDeviceSelectionTests passed\n";
  return 0;
}
