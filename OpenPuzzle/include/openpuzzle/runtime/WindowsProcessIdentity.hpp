#pragma once
#include <cstdint>
#include <optional>

namespace openpuzzle {
// FILETIME creation values are Windows-specific, not Linux /proc start ticks.
// Persistent runtime records must identify their platform before comparing them.
class WindowsProcessIdentity {
public:
  using CreationTime = std::uint64_t;
  static std::optional<CreationTime> creationTime(std::uint32_t pid);
  // Query and termination refer to the same open process object, avoiding PID reuse.
  static bool terminateIfMatches(std::uint32_t pid, CreationTime expected,
                                 std::uint32_t exitCode = 124);
};
} // namespace openpuzzle
