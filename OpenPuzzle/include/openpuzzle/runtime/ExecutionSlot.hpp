#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace openpuzzle {

class ExecutionSlot {
public:
  static bool valid(const std::string& value);

  static std::string cuda(std::uint64_t device);

  static std::optional<std::uint64_t>
  cudaDevice(const std::string& value);

  /*
   * Returns the safe filename suffix for a recognised slot.
   * "primary" and unknown legacy values retain the unsuffixed path.
   */
  static std::string fileSuffix(
      const std::string& value);
};

} // namespace openpuzzle
