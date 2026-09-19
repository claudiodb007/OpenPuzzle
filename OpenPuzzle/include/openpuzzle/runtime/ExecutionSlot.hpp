#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

class ExecutionSlot {
public:
  static bool valid(const std::string& value);

  static std::string cuda(std::uint64_t device);

  static std::string opencl(std::uint64_t device);

  static std::optional<std::uint64_t>
  cudaDevice(const std::string& value);

  static std::optional<std::uint64_t>
  openclDevice(const std::string& value);

  /*
   * Returns the safe filename suffix for a recognised slot.
   * "primary" and unknown legacy values retain the unsuffixed path.
   */
  static std::string fileSuffix(
      const std::string& value);

  static std::vector<std::string>
  discoverCudaSlots(
      const std::filesystem::path& directory);

  static std::vector<std::string>
  discoverGpuSlots(
      const std::filesystem::path& directory);
};

} // namespace openpuzzle
