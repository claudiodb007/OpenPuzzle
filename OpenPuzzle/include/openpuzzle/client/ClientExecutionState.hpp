#pragma once

#include <cstdint>
#include <string>

namespace openpuzzle::client {

struct ClientExecutionState {
  bool active = false;

  std::string assignmentId;
  std::string clientId;

  int puzzle = 0;
  int rangeId = 0;
  int pid = 0;

  /*
   * Linux boot identifier captured when the execution starts.
   *
   * A PID alone is not a stable process identity across reboots because
   * Linux may reuse the same numeric PID after the machine starts again.
   */
  std::string bootId;

  /*
   * Linux /proc/<pid>/stat field 22 captured for this exact
   * process instance.
   *
   * PID + boot_id is not sufficient against PID reuse within the
   * same system boot. processStartTime distinguishes two different
   * processes that happened to receive the same numeric PID.
   *
   * Zero means legacy state or unavailable identity and must never
   * be treated as positive proof that the original process is alive.
   */
  std::uint64_t processStartTime = 0;

  int device = 0;
  int blocks = 0;
  int threads = 0;
  int points = 0;

  bool profileManaged = false;

  std::string target;
  std::string publicKey;

  // Explicit PSCKangaroo walk identity for this process generation.
  std::string kangarooWalkSeed;
  std::uint64_t kangarooGeneration = 0;

  std::string start;
  std::string end;

  std::string engine;
  std::string backend;
  std::string gpuName;

  std::string workspace;
  std::string command;

  bool valid() const {
    return active &&
           !assignmentId.empty() &&
           !clientId.empty() &&
           puzzle > 0 &&
           rangeId > 0 &&
           pid > 0 &&
           !workspace.empty();
  }
};

} // namespace openpuzzle::client
