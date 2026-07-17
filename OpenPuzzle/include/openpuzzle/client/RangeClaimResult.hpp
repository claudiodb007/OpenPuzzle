#pragma once

#include "openpuzzle/client/RangeAssignment.hpp"

#include <optional>
#include <string>

namespace openpuzzle::client {

enum class RangeClaimStatus {
  Assigned,
  Unavailable,
  Failed
};

struct RangeClaimResult {
  RangeClaimStatus status =
      RangeClaimStatus::Failed;

  std::optional<RangeAssignment> assignment;

  std::string message;

  bool assigned() const {
    return status == RangeClaimStatus::Assigned &&
           assignment.has_value();
  }

  bool unavailable() const {
    return status == RangeClaimStatus::Unavailable;
  }

  bool failed() const {
    return status == RangeClaimStatus::Failed;
  }
};

} // namespace openpuzzle::client
