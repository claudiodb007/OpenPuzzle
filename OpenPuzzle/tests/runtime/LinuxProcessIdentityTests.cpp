#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"

#include <cassert>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <unistd.h>

using openpuzzle::LinuxProcessIdentity;

int main() {
  /*
   * Synthetic /proc stat record.
   *
   * Fields:
   * 1 pid
   * 2 comm
   * 3..21 arbitrary valid scalar tokens
   * 22 starttime = 42424242
   */
  {
    const std::string stat =
        "123 (openpuzzle) "
        "S "
        "1 2 3 4 5 6 7 8 9 10 "
        "11 12 13 14 15 16 17 18 "
        "42424242 "
        "20 21 22";

    const auto parsed =
        LinuxProcessIdentity::
            parseStartTime(stat);

    assert(parsed);
    assert(*parsed == 42424242ULL);
  }

  /*
   * comm may contain whitespace.
   */
  {
    const std::string stat =
        "123 (OpenPuzzle worker process) "
        "S "
        "1 2 3 4 5 6 7 8 9 10 "
        "11 12 13 14 15 16 17 18 "
        "987654321 "
        "20";

    const auto parsed =
        LinuxProcessIdentity::
            parseStartTime(stat);

    assert(parsed);
    assert(*parsed == 987654321ULL);
  }

  /*
   * comm may itself contain ')'. Using the final closing parenthesis
   * is necessary for correct parsing.
   */
  {
    const std::string stat =
        "123 (OpenPuzzle ) worker) "
        "R "
        "1 2 3 4 5 6 7 8 9 10 "
        "11 12 13 14 15 16 17 18 "
        "123456789 "
        "20";

    const auto parsed =
        LinuxProcessIdentity::
            parseStartTime(stat);

    assert(parsed);
    assert(*parsed == 123456789ULL);
  }

  /*
   * Invalid and truncated records must fail closed.
   */
  {
    assert(
        !LinuxProcessIdentity::
            parseStartTime(""));

    assert(
        !LinuxProcessIdentity::
            parseStartTime(
                "123 openpuzzle S 1 2 3"));

    assert(
        !LinuxProcessIdentity::
            parseStartTime(
                "123 (openpuzzle) S 1 2 3"));

    assert(
        !LinuxProcessIdentity::
            parseStartTime(
                "123 (openpuzzle) "
                "S "
                "1 2 3 4 5 6 7 8 9 10 "
                "11 12 13 14 15 16 17 18 "
                "not-a-number"));
  }

  /*
   * A real live process must expose a stable, non-zero starttime.
   */
  {
    const int pid =
        static_cast<int>(getpid());

    const auto first =
        LinuxProcessIdentity::
            startTime(pid);

    const auto second =
        LinuxProcessIdentity::
            startTime(pid);

    assert(first);
    assert(second);
    assert(*first > 0);
    assert(*first == *second);
  }

  /*
   * Invalid/non-existent PIDs cannot produce identity.
   */
  {
    assert(
        !LinuxProcessIdentity::
            startTime(0));

    assert(
        !LinuxProcessIdentity::
            startTime(-1));

    assert(
        !LinuxProcessIdentity::
            startTime(999999999));
  }


  /*
   * pidfd signalling with signal 0 validates the exact process without
   * delivering a terminating signal.
   */
  {
    const int self =
        static_cast<int>(
            getpid());

    const auto selfStartTime =
        LinuxProcessIdentity::
            startTime(self);

    assert(selfStartTime);
    assert(*selfStartTime > 0);

    assert(
        LinuxProcessIdentity::
            signalIfMatches(
                self,
                *selfStartTime,
                0));

    assert(
        !LinuxProcessIdentity::
             signalIfMatches(
                 self,
                 *selfStartTime + 1,
                 0));
  }

  std::cout
      << "LinuxProcessIdentityTests passed\n";

  return 0;
}
