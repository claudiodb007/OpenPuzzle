#include "openpuzzle/engines/kangaroo/KangarooMemoryPlanner.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <unistd.h>

namespace openpuzzle {
namespace {

constexpr std::uint64_t gibibyte =
    1024ULL * 1024ULL * 1024ULL;

std::optional<std::uint64_t>
readCgroupLimit(
    const std::filesystem::path &path) {
  std::ifstream input(path);
  std::string value;

  if (!input || !(input >> value) ||
      value.empty() || value == "max") {
    return std::nullopt;
  }

  std::size_t consumed = 0;

  try {
    const auto limit =
        std::stoull(value, &consumed);

    if (consumed != value.size() ||
        limit == 0 ||
        limit >= (1ULL << 60)) {
      return std::nullopt;
    }

    return limit;
  } catch (...) {
    return std::nullopt;
  }
}

std::uint64_t physicalMemoryBytes() {
  const long pages =
      sysconf(_SC_PHYS_PAGES);

  const long pageSize =
      sysconf(_SC_PAGESIZE);

  if (pages <= 0 || pageSize <= 0) {
    return 0;
  }

  const auto pageCount =
      static_cast<std::uint64_t>(pages);

  const auto bytesPerPage =
      static_cast<std::uint64_t>(pageSize);

  if (pageCount >
      std::numeric_limits<std::uint64_t>::max() /
          bytesPerPage) {
    return 0;
  }

  return pageCount * bytesPerPage;
}

} // namespace

std::uint64_t
KangarooMemoryPlanner::systemMemoryLimitBytes() {
  std::uint64_t result =
      physicalMemoryBytes();

  for (const auto &path : {
           std::filesystem::path(
               "/sys/fs/cgroup/memory.max"),
           std::filesystem::path(
               "/sys/fs/cgroup/memory/memory.limit_in_bytes"),
       }) {
    const auto cgroup =
        readCgroupLimit(path);

    if (cgroup &&
        (result == 0 || *cgroup < result)) {
      result = *cgroup;
    }
  }

  return result;
}

int KangarooMemoryPlanner::recommendedRamLimitGiB(
    std::uint64_t memoryBytes) {
  if (memoryBytes == 0) {
    return 8;
  }

  const auto totalGiB =
      std::max<std::uint64_t>(
          1,
          memoryBytes / gibibyte);

  auto limitGiB =
      std::max<std::uint64_t>(
          1,
          (totalGiB * 3) / 5);

  if (limitGiB >= 4) {
    limitGiB =
        (limitGiB / 4) * 4;
  }

  limitGiB =
      std::min<std::uint64_t>(
          limitGiB,
          static_cast<std::uint64_t>(
              std::numeric_limits<int>::max()));

  return static_cast<int>(limitGiB);
}

int KangarooMemoryPlanner::recommendedRamLimitGiB() {
  return recommendedRamLimitGiB(
      systemMemoryLimitBytes());
}

} // namespace openpuzzle
