#pragma once

#include <string>

namespace openpuzzle::client {

struct RangeAssignment {
  std::string assignmentId;

  int puzzle = 0;
  int rangeId = 0;

  std::string target;
  std::string start;
  std::string end;

  bool valid() const {
    return !assignmentId.empty() &&
           puzzle > 0 &&
           rangeId > 0 &&
           !target.empty() &&
           !start.empty() &&
           !end.empty();
  }
};

} // namespace openpuzzle::client
