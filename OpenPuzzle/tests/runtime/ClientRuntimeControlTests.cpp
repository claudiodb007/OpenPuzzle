#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/RunSession.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

int main() {
  const char *originalHome =
      std::getenv("HOME");

  const bool hadHome =
      originalHome != nullptr;

  const std::string savedHome =
      hadHome
          ? originalHome
          : "";

  const auto temporaryHome =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-runtime-control-" +
          std::to_string(getpid())
      );

  std::filesystem::remove_all(
      temporaryHome);

  std::filesystem::create_directories(
      temporaryHome);

  assert(
      setenv(
          "HOME",
          temporaryHome.string().c_str(),
          1) == 0);

  assert(!ClientRuntimeControl::running());
  assert(!ClientRuntimeControl::runtimePid());
  assert(
      ClientRuntimeControl::pidPath()
          .filename() == "runtime.pid");

  assert(
      ClientRuntimeControl::
          pidPath("gpu")
              .filename() ==
      "runtime-gpu.pid");

  assert(
      ClientRuntimeControl::
          pidPath("cpu")
              .filename() ==
      "runtime-cpu.pid");
  assert(
      ClientRuntimeControl::
          pidPath("cuda")
              .filename() ==
      "runtime-cuda.pid");
  assert(
      ClientRuntimeControl::
          pidPath("opencl")
              .filename() ==
      "runtime-opencl.pid");

  assert(
      ClientRuntimeControl::
          safeStopPath("gpu")
              .filename() ==
      "safestop-gpu.requested");

  assert(
      ClientRuntimeControl::
          safeStopPath("cpu")
              .filename() ==
      "safestop-cpu.requested");
  assert(
      ClientRuntimeControl::
          safeStopPath("cuda")
              .filename() ==
      "safestop-cuda.requested");
  assert(
      ClientRuntimeControl::
          safeStopPath("opencl")
              .filename() ==
      "safestop-opencl.requested");

  const std::vector<std::string>
      concurrentArguments = {
          "run",
          "71",
          "--with-cpu",
          "--cpu-threads",
          "1",
          "--backend",
          "opencl",
          "--device",
          "1",
          "--rusticl-enable",
          "radeonsi",
          "--server",
          "https://example.test",
      };

  const std::vector<std::string>
      expectedGpuArguments = {
          "run",
          "71",
          "--backend",
          "opencl",
          "--device",
          "1",
          "--rusticl-enable",
          "radeonsi",
          "--server",
          "https://example.test",
      };

  const std::vector<std::string>
      expectedCpuArguments = {
          "run",
          "71",
          "--server",
          "https://example.test",
          "--backend",
          "cpu",
          "--threads",
          "1",
      };

  assert(
      RunSession::
          concurrentGpuArguments(
              concurrentArguments) ==
      expectedGpuArguments);

  assert(
      RunSession::
          concurrentCpuArguments(
              concurrentArguments) ==
      expectedCpuArguments);

  const std::vector<std::string>
      cudaOpenclArguments = {
          "run",
          "71",
          "--backend",
          "cuda",
          "--device",
          "0",
          "--with-opencl",
          "--opencl-device",
          "1",
          "--rusticl-enable",
          "radeonsi",
          "--server",
          "https://example.test",
      };

  const std::vector<std::string>
      expectedCudaArguments = {
          "run",
          "71",
          "--backend",
          "cuda",
          "--device",
          "0",
          "--server",
          "https://example.test",
      };

  const std::vector<std::string>
      expectedOpenclArguments = {
          "run",
          "71",
          "--server",
          "https://example.test",
          "--backend",
          "opencl",
          "--device",
          "1",
          "--rusticl-enable",
          "radeonsi",
      };

  assert(
      RunSession::
          concurrentCudaArguments(
              cudaOpenclArguments) ==
      expectedCudaArguments);

  assert(
      RunSession::
          concurrentOpenclArguments(
              cudaOpenclArguments) ==
      expectedOpenclArguments);

  auto expectedCudaPreflight =
      expectedCudaArguments;

  expectedCudaPreflight.push_back(
      "--preflight-only");

  assert(
      RunSession::
          concurrentPreflightArguments(
              expectedCudaArguments) ==
      expectedCudaPreflight);

  GpuInfo cudaDevice;
  cudaDevice.device = 0;
  cudaDevice.name =
      "NVIDIA GeForce RTX 4070 SUPER";
  cudaDevice.backend = "CUDA";
  cudaDevice.memoryMb = 12282;
  cudaDevice.cuda = true;

  GpuInfo openclNvidia;
  openclNvidia.device = 0;
  openclNvidia.name =
      "NVIDIA GeForce RTX 4070 SUPER";
  openclNvidia.backend = "OpenCL";
  openclNvidia.memoryMb = 11876;
  openclNvidia.opencl = true;

  GpuInfo openclAmd;
  openclAmd.device = 1;
  openclAmd.name =
      "AMD Radeon RX 5500 XT "
      "(radeonsi, navi14, ACO)";
  openclAmd.backend = "OpenCL";
  openclAmd.memoryMb = 8192;
  openclAmd.opencl = true;

  const std::vector<GpuInfo> cudaDevices = {
      cudaDevice,
  };

  const std::vector<GpuInfo> openclDevices = {
      openclNvidia,
      openclAmd,
  };

  RunSession::validateConcurrentGpuSelection(
      expectedCudaArguments,
      expectedOpenclArguments,
      cudaDevices,
      openclDevices);

  auto sameGpuOpenclArguments =
      expectedOpenclArguments;

  for (std::size_t index = 0;
       index + 1 < sameGpuOpenclArguments.size();
       ++index) {
    if (sameGpuOpenclArguments[index] ==
        "--device") {
      sameGpuOpenclArguments[index + 1] = "0";
      break;
    }
  }

  bool sameGpuRejected = false;

  try {
    RunSession::validateConcurrentGpuSelection(
        expectedCudaArguments,
        sameGpuOpenclArguments,
        cudaDevices,
        openclDevices);
  } catch (const std::runtime_error &error) {
    sameGpuRejected =
        std::string(error.what()).find(
            "same physical GPU") !=
        std::string::npos;
  }

  assert(sameGpuRejected);

  auto missingOpenclArguments =
      expectedOpenclArguments;

  for (std::size_t index = 0;
       index + 1 < missingOpenclArguments.size();
       ++index) {
    if (missingOpenclArguments[index] ==
        "--device") {
      missingOpenclArguments[index + 1] = "2";
      break;
    }
  }

  bool missingGpuRejected = false;

  try {
    RunSession::validateConcurrentGpuSelection(
        expectedCudaArguments,
        missingOpenclArguments,
        cudaDevices,
        openclDevices);
  } catch (const std::runtime_error &error) {
    missingGpuRejected =
        std::string(error.what()).find(
            "OpenCL device 2 was not found") !=
        std::string::npos;
  }

  assert(missingGpuRejected);

  assert(
      setenv(
          "OPENPUZZLE_EXECUTION_SLOT",
          "gpu",
          1) == 0);
  assert(
      ClientRuntimeControl::pidPath()
          .filename() == "runtime-gpu.pid");

  assert(ClientRuntimeControl::acquire());
  assert(ClientRuntimeControl::running());

  const auto pid =
      ClientRuntimeControl::runtimePid();

  assert(pid);
  assert(*pid == static_cast<int>(getpid()));

  assert(
      ClientRuntimeControl::
          requestSafeStop());
  assert(
      ClientRuntimeControl::
          safeStopRequested());
  assert(
      ClientRuntimeControl::
          clearSafeStop());
  assert(
      !ClientRuntimeControl::
           safeStopRequested());

  /*
   * A mesma ou outra instância não pode adquirir
   * o controlo enquanto o PID estiver ativo.
   */
  assert(!ClientRuntimeControl::acquire());

  assert(ClientRuntimeControl::release());
  assert(!ClientRuntimeControl::runtimePid());
  assert(!ClientRuntimeControl::running());

  assert(
      !ClientRuntimeControl::
           requestSafeStop());

  /*
   * Um PID obsoleto é limpo automaticamente.
   */
  std::filesystem::create_directories(
      ClientRuntimeControl::pidPath()
          .parent_path());

  {
    std::ofstream output(
        ClientRuntimeControl::pidPath());

    output << "999999999\n";
  }

  assert(!ClientRuntimeControl::requestStop());
  assert(!ClientRuntimeControl::runtimePid());

  assert(ClientRuntimeControl::acquire());
  assert(ClientRuntimeControl::release());

  assert(
      setenv(
          "OPENPUZZLE_EXECUTION_SLOT",
          "cpu",
          1) == 0);
  assert(
      ClientRuntimeControl::pidPath()
          .filename() == "runtime-cpu.pid");
  assert(ClientRuntimeControl::acquire());
  assert(ClientRuntimeControl::release());

  assert(
      unsetenv(
          "OPENPUZZLE_EXECUTION_SLOT") == 0);

  std::filesystem::remove_all(
      temporaryHome);

  if (hadHome) {
    assert(
        setenv(
            "HOME",
            savedHome.c_str(),
            1) == 0);
  } else {
    assert(
        unsetenv("HOME") == 0);
  }

  std::cout
      << "ClientRuntimeControlTests passed\n";

  return 0;
}
