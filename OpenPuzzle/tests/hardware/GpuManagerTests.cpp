#include "openpuzzle/hardware/GpuManager.hpp"

#include <string>

using namespace openpuzzle;

int main() {
  const std::string output =
      "ID:     0\n"
      "Name:   NVIDIA Test GPU\n"
      "Memory: 12288MB\n"
      "Compute units: 56\n"
      "\n"
      "ID:     1\n"
      "Name:   AMD Test GPU\n"
      "Memory: 16384MB\n"
      "Compute units: 60\n";

  const auto cuda =
      GpuManager::parseBitCrackDevices(
          output,
          "CUDA");

  if (cuda.size() != 2)
    return 1;

  if (cuda[0].device != 0)
    return 2;

  if (cuda[0].name !=
      "NVIDIA Test GPU")
    return 3;

  if (cuda[0].memoryMb != 12288)
    return 4;

  if (cuda[0].computeUnits != 56)
    return 5;

  if (!cuda[0].cuda)
    return 6;

  if (cuda[0].opencl)
    return 7;

  if (cuda[1].device != 1)
    return 8;

  if (cuda[1].computeUnits != 60)
    return 9;

  const auto opencl =
      GpuManager::parseBitCrackDevices(
          output,
          "OpenCL");

  if (opencl.size() != 2)
    return 10;

  if (!opencl[0].opencl)
    return 11;

  if (opencl[0].cuda)
    return 12;

  /*
   * Missing optional values must remain safe.
   */
  const auto minimal =
      GpuManager::parseBitCrackDevices(
          "ID: 3\n"
          "Name: Generic GPU\n",
          "OpenCL");

  if (minimal.size() != 1)
    return 13;

  if (minimal[0].device != 3)
    return 14;

  if (minimal[0].computeUnits != 0)
    return 15;

  if (minimal[0].memoryMb != 0)
    return 16;

  return 0;
}
