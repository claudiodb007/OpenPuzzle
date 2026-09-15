#include "openpuzzle/engines/kangaroo/KangarooMemoryPlanner.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace openpuzzle;

namespace {

constexpr std::uint64_t gibibyte =
    1024ULL * 1024ULL * 1024ULL;

} // namespace

int main() {
  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB(0) == 8);

  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB(4 * gibibyte) == 2);

  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB(8 * gibibyte) == 4);

  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB(16 * gibibyte) == 8);

  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB(32 * gibibyte) == 16);

  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB(62 * gibibyte) == 36);

  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB(128 * gibibyte) == 76);

  assert(
      KangarooMemoryPlanner::
          recommendedRamLimitGiB() >= 1);

  std::cout
      << "KangarooMemoryPlannerTests passed\n";

  return 0;
}
