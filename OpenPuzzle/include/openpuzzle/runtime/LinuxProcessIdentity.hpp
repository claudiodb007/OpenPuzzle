#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace openpuzzle {

/**
 * Linux process identity helpers.
 *
 * Numeric PIDs are recyclable. On Linux, /proc/<pid>/stat field 22
 * ("starttime") identifies when that particular process instance was
 * created, measured in clock ticks since system boot.
 *
 * Combining:
 *
 *   boot_id + pid + starttime
 *
 * gives OpenPuzzle a persistent process identity that is safe against
 * PID reuse both across reboots and within the same boot.
 */
class LinuxProcessIdentity {
public:
  using StartTime = std::uint64_t;

  /**
   * Reads field 22 (starttime) from /proc/<pid>/stat.
   *
   * Returns std::nullopt when:
   * - pid is invalid;
   * - /proc/<pid>/stat cannot be opened;
   * - the stat record is malformed;
   * - starttime cannot be parsed.
   */
  static std::optional<StartTime> startTime(int pid);

  /**
   * Parses Linux /proc/<pid>/stat content and returns field 22.
   *
   * Exposed separately so parser edge cases can be tested without
   * depending on a particular live process.
   */
  static std::optional<StartTime> parseStartTime(
      const std::string& statLine);

  /*
   * Send a signal through a pidfd only if the numeric PID still refers
   * to the exact process instance represented by expectedStartTime.
   *
   * The pidfd is ephemeral and is never persisted.
   */
  static bool signalIfMatches(
      int pid,
      StartTime expectedStartTime,
      int signal);

};

} // namespace openpuzzle
