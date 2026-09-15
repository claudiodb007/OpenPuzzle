#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/RunSession.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"

#include <cassert>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
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

  RunSession runSession;

  assert(
      runSession.run({
          "run",
          "140",
          "--engine",
          "kangaroo",
          "--backend",
          "cuda",
          "--device",
          "0",
          "--with-opencl",
          "--opencl-device",
          "1",
      }) == 1);

  assert(
      runSession.run({
          "run",
          "140",
          "--backend",
          "cuda",
          "--device",
          "0",
          "--with-opencl",
          "--opencl-device",
          "1",
      }) == 1);

  assert(
      runSession.run({
          "run",
          "140",
          "--engine",
          "KANGAROO",
          "--with-cpu",
          "--cpu-threads",
          "1",
      }) == 1);

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

  const auto runtimeBootId =
      ClientRuntimeControl::runtimeBootId();

  assert(runtimeBootId);
  assert(
      *runtimeBootId ==
      client::ClientStateStore::currentBootId());

  const auto runtimeStartTime =
      ClientRuntimeControl::
          runtimeStartTime();

  const auto selfStartTime =
      LinuxProcessIdentity::
          startTime(
              static_cast<int>(
                  getpid()));

  assert(runtimeStartTime);
  assert(selfStartTime);

  assert(
      *runtimeStartTime ==
      *selfStartTime);

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
   * PID existente mas pertencente a outro boot:
   *
   * nunca pode ser considerado runtime ativo e
   * nunca pode receber SIGTERM/safestop.
   */
  {
    std::ofstream output(
        ClientRuntimeControl::pidPath());

    output
        << static_cast<int>(getpid())
        << "\n"
        << "00000000-0000-0000-0000-000000000000"
        << "\n";
  }

  assert(!ClientRuntimeControl::running());

  assert(
      !ClientRuntimeControl::
           requestSafeStop());

  /*
   * requestStop apenas remove o marcador obsoleto.
   * O processo atual obviamente continua vivo.
   */
  assert(
      !ClientRuntimeControl::
           requestStop());

  assert(!ClientRuntimeControl::runtimePid());

  assert(
      kill(
          static_cast<int>(getpid()),
          0) == 0);

  /*
   * Um PID obsoleto é limpo automaticamente.
   */
  std::filesystem::create_directories(
      ClientRuntimeControl::pidPath()
          .parent_path());

  {
    std::ofstream output(
        ClientRuntimeControl::pidPath());

    output
        << "999999999\n"
        << client::ClientStateStore::currentBootId()
        << "\n";
  }

  assert(!ClientRuntimeControl::requestStop());
  assert(!ClientRuntimeControl::runtimePid());

  /*
   * Marcador OpenPuzzle 1.0.17:
   *
   * PID + boot_id sem starttime já não constitui identidade completa.
   * Mesmo apontando para este processo vivo, deve falhar fechado e ser
   * tratado como marcador obsoleto.
   */
  {
    std::ofstream output(
        ClientRuntimeControl::pidPath());

    output
        << static_cast<int>(getpid())
        << "\n"
        << client::ClientStateStore::
               currentBootId()
        << "\n";
  }

  assert(
      !ClientRuntimeControl::running());

  assert(
      !ClientRuntimeControl::
           runtimeStartTime());

  assert(
      !ClientRuntimeControl::
           requestSafeStop());

  assert(
      !ClientRuntimeControl::
           requestStop());

  assert(
      !ClientRuntimeControl::
           runtimePid());

  assert(
      kill(
          static_cast<int>(getpid()),
          0) == 0);

  /*
   * Marcador legado <= 1.0.16 contém apenas PID.
   * Não é identidade suficiente e deve ser limpo.
   */
  {
    std::ofstream output(
        ClientRuntimeControl::pidPath());

    output
        << static_cast<int>(getpid())
        << "\n";
  }

  assert(!ClientRuntimeControl::running());
  assert(!ClientRuntimeControl::requestStop());
  assert(!ClientRuntimeControl::runtimePid());

  /*
   * Real pidfd stop path.
   *
   * Persist the exact identity of a child and verify that requestStop()
   * terminates that process through pidfd signalling.
   */
  {
    const pid_t child =
        fork();

    assert(child >= 0);

    if (child == 0) {
      for (;;) {
        pause();
      }
    }

    std::optional<
        LinuxProcessIdentity::StartTime>
        childStartTime;

    for (int attempt = 0;
         attempt < 100 &&
         !childStartTime;
         ++attempt) {
      childStartTime =
          LinuxProcessIdentity::
              startTime(
                  static_cast<int>(
                      child));

      if (!childStartTime) {
        usleep(10000);
      }
    }

    assert(childStartTime);

    {
      std::ofstream output(
          ClientRuntimeControl::pidPath());

      output
          << static_cast<int>(child)
          << "\n"
          << client::ClientStateStore::
                 currentBootId()
          << "\n"
          << *childStartTime
          << "\n";
    }

    assert(
        ClientRuntimeControl::running());

    assert(
        ClientRuntimeControl::
            requestStop());

    int status = 0;

    assert(
        waitpid(
            child,
            &status,
            0) == child);

    assert(WIFSIGNALED(status));
    assert(
        WTERMSIG(status) ==
        SIGTERM);

    std::error_code error;

    std::filesystem::remove(
        ClientRuntimeControl::pidPath(),
        error);
  }


  /*
   * Same-boot PID reuse:
   *
   * PID and boot_id both match this live process but starttime does not.
   * running() must reject it and requestStop() must never signal us.
   */
  {
    const auto currentStartTime =
        LinuxProcessIdentity::
            startTime(
                static_cast<int>(
                    getpid()));

    assert(currentStartTime);

    std::ofstream output(
        ClientRuntimeControl::pidPath());

    output
        << static_cast<int>(getpid())
        << "\n"
        << client::ClientStateStore::
               currentBootId()
        << "\n"
        << (*currentStartTime + 1)
        << "\n";
  }

  assert(
      !ClientRuntimeControl::running());

  assert(
      !ClientRuntimeControl::
           requestSafeStop());

  assert(
      !ClientRuntimeControl::
           requestStop());

  assert(
      !ClientRuntimeControl::
           runtimePid());

  assert(
      kill(
          static_cast<int>(getpid()),
          0) == 0);

  assert(ClientRuntimeControl::acquire());
  assert(ClientRuntimeControl::release());

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
