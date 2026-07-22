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
  int threads = 0;

  std::string target;
  std::string start;
  std::string end;

  std::string engine;
  std::string backend;

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
