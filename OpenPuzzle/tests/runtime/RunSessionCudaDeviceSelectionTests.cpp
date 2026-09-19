#include "openpuzzle/runtime/RunSession.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using openpuzzle::GpuInfo;
using openpuzzle::RunSession;

namespace {

GpuInfo cudaGpu(
    const int device) {
  GpuInfo gpu;
  gpu.device = device;
  gpu.name = "GPU " + std::to_string(device);
  gpu.backend = "CUDA";
  gpu.memoryMb = 10240;
  gpu.cuda = true;
  return gpu;
}

void expectFailure(
    const std::vector<std::string>& arguments,
    const std::vector<GpuInfo>& available,
    const std::string& message) {
  try {
    (void) RunSession::selectedCudaDevices(
        arguments,
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
      cudaGpu(5),
      cudaGpu(2),
      cudaGpu(0),
      cudaGpu(4),
      cudaGpu(1),
      cudaGpu(3),
  };

  assert(
      RunSession::selectedCudaDevices(
          {
              "run",
              "71",
              "--backend",
              "cuda",
              "--engine",
              "bitcrack",
              "--devices",
              "all",
          },
          sixGpus) ==
      std::vector<int>({0, 1, 2, 3, 4, 5}));

  assert(
      RunSession::selectedCudaDevices(
          {
              "run",
              "71",
              "--devices",
              "5,0,2",
          },
          sixGpus) ==
      std::vector<int>({5, 0, 2}));

  assert(
      RunSession::cudaWorkerArguments(
          {
              "run",
              "71",
              "--devices",
              "all",
              "--duration-minutes",
              "60",
          },
          4) ==
      std::vector<std::string>({
          "run",
          "71",
          "--duration-minutes",
          "60",
          "--backend",
          "cuda",
          "--engine",
          "bitcrack",
          "--device",
          "4",
          "--supervised-gpu-device",
      }));

  assert(
      RunSession::selectedOpenclDevices(
          {
              "run",
              "71",
              "--backend",
              "opencl",
              "--engine",
              "bitcrack",
              "--devices",
              "5,0,2",
          },
          sixGpus) ==
      std::vector<int>({5, 0, 2}));

  assert(
      RunSession::openclWorkerArguments(
          {
              "run",
              "71",
              "--backend",
              "opencl",
              "--devices",
              "all",
              "--rusticl-enable",
              "radeonsi",
          },
          2) ==
      std::vector<std::string>({
          "run",
          "71",
          "--backend",
          "opencl",
          "--rusticl-enable",
          "radeonsi",
          "--engine",
          "bitcrack",
          "--device",
          "2",
          "--supervised-gpu-device",
      }));

  assert(
      RunSession::cudaWorkerArguments(
          {
              "run",
              "71",
              "--backend",
              "cuda",
              "--engine",
              "bitcrack",
              "--devices",
              "5,3,1",
              "--once",
          },
          3) ==
      std::vector<std::string>({
          "run",
          "71",
          "--backend",
          "cuda",
          "--engine",
          "bitcrack",
          "--once",
          "--device",
          "3",
          "--supervised-gpu-device",
      }));

  assert(
      RunSession::concurrentPreflightArguments(
          RunSession::cudaWorkerArguments(
              {
                  "run",
                  "71",
                  "--devices",
                  "0,1",
              },
              1)) ==
      std::vector<std::string>({
          "run",
          "71",
          "--backend",
          "cuda",
          "--engine",
          "bitcrack",
          "--device",
          "1",
          "--supervised-gpu-device",
          "--preflight-only",
      }));

  try {
    (void) RunSession::cudaWorkerArguments(
        {"run", "71", "--devices", "all"},
        -1);
    assert(false);
  } catch (const std::runtime_error& error) {
    assert(
        std::string(error.what()).find("negative") !=
        std::string::npos);
  }

  expectFailure(
      {"run", "71"},
      sixGpus,
      "--devices is required");
  expectFailure(
      {
          "run",
          "71",
          "--devices",
          "0",
          "--devices",
          "1",
      },
      sixGpus,
      "only be specified once");
  expectFailure(
      {
          "run",
          "71",
          "--device",
          "0",
          "--devices",
          "all",
      },
      sixGpus,
      "cannot be combined");
  expectFailure(
      {
          "run",
          "71",
          "--devices",
          "all",
          "--with-opencl",
      },
      sixGpus,
      "cannot be combined");
  expectFailure(
      {
          "run",
          "71",
          "--backend",
          "opencl",
          "--devices",
          "all",
      },
      sixGpus,
      "requires --backend cuda");
  expectFailure(
      {
          "run",
          "140",
          "--engine",
          "kangaroo",
          "--devices",
          "all",
      },
      sixGpus,
      "requires --engine bitcrack");

  std::cout
      << "RunSessionCudaDeviceSelectionTests passed\n";
  return 0;
}
