#include "openpuzzle/services/DoctorService.hpp"

#include "openpuzzle/hardware/GpuInfo.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace openpuzzle {

namespace {

int logicalProcessorCount() {
  const long processors =
      sysconf(_SC_NPROCESSORS_ONLN);

  return processors > 0
      ? static_cast<int>(processors)
      : 1;
}

std::string availability(
    bool available) {
  return available
      ? "READY"
      : "NOT AVAILABLE";
}

void printEngine(
    const std::string& label,
    const std::optional<std::string>& path) {
  std::cout
      << label
      << (path ? "OK" : "MISSING")
      << '\n';

  if (path) {
    std::cout
        << "Executable.......... "
        << *path
        << '\n';
  }
}

void printDevices(
    const std::string& backend,
    const std::vector<GpuInfo>& devices) {
  std::cout
      << backend
      << " devices....... "
      << devices.size()
      << '\n';

  for (const auto& device : devices) {
    std::cout
        << "Device.............. "
        << device.device
        << ": "
        << device.name
        << '\n';

    if (device.memoryMb > 0) {
      std::cout
          << "Memory.............. "
          << device.memoryMb
          << " MiB\n";
    }
  }
}

} // namespace

int DoctorService::execute(
    const std::vector<std::string>&) const {
  const auto cudaEngine =
      ToolManager::bitcrackCudaPath();

  const auto openclEngine =
      ToolManager::bitcrackOpenCLPath();

  const auto cpuEngine =
      ToolManager::keyhuntPath();

  const auto cudaDevices =
      GpuManager::listCudaGpus();

  const auto openclDevices =
      GpuManager::listOpenClGpus();

  const int processors =
      logicalProcessorCount();

  const bool cudaReady =
      cudaEngine.has_value() &&
      !cudaDevices.empty();

  const bool openclReady =
      openclEngine.has_value() &&
      !openclDevices.empty();

  const bool cpuReady =
      cpuEngine.has_value() &&
      processors > 0;

  const bool ready =
      cudaReady ||
      openclReady ||
      cpuReady;

  const auto configurationPath =
      std::filesystem::path(
          ToolManager::configPath());

  std::error_code error;

  const bool configurationPresent =
      std::filesystem::is_regular_file(
          configurationPath,
          error);

  std::cout
      << "OpenPuzzle Doctor\n"
      << "-----------------\n"
      << "Configuration...... "
      << (
             configurationPresent
                 ? "available"
                 : "not created")
      << '\n'
      << "Configuration path. "
      << configurationPath.string()
      << "\n\n"
      << "Bundled engines\n"
      << "---------------\n";

  printEngine(
      "CUDA engine........ ",
      cudaEngine);

  printEngine(
      "OpenCL engine...... ",
      openclEngine);

  printEngine(
      "CPU engine......... ",
      cpuEngine);

  std::cout
      << "\nHardware\n"
      << "--------\n"
      << "Logical processors. "
      << processors
      << '\n';

  printDevices(
      "CUDA",
      cudaDevices);

  printDevices(
      "OpenCL",
      openclDevices);

  std::cout
      << "\nUsable backends\n"
      << "---------------\n"
      << "CUDA backend....... "
      << availability(cudaReady)
      << '\n'
      << "OpenCL backend..... "
      << availability(openclReady)
      << '\n'
      << "CPU backend........ "
      << availability(cpuReady)
      << "\n\n"
      << "Status............. "
      << (
             ready
                 ? "READY"
                 : "NOT READY")
      << '\n';

  return ready
      ? 0
      : 1;
}

} // namespace openpuzzle
