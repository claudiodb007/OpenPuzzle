#pragma once

#include <cstdint>

namespace openpuzzle {

class KangarooMemoryPlanner {
public:
  static int recommendedRamLimitGiB();

  static int recommendedRamLimitGiB(
      std::uint64_t memoryBytes);

  static std::uint64_t systemMemoryLimitBytes();
};

} // namespace openpuzzle
