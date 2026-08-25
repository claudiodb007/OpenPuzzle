#pragma once

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

  int device = 0;
  int blocks = 0;
  int threads = 0;
  int points = 0;

  bool profileManaged = false;

  std::string target;
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
