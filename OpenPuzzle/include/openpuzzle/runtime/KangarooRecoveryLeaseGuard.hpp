#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <string>

namespace openpuzzle {

enum class KangarooRecoveryLeaseStatus {
  Accepted,
  Retry,
  Rejected,
  Invalid
};

struct KangarooRecoveryLeaseResult {
  KangarooRecoveryLeaseStatus status =
      KangarooRecoveryLeaseStatus::Invalid;
  double speedMKeys = 0.0;
  std::string error;

  bool accepted() const {
    return status == KangarooRecoveryLeaseStatus::Accepted;
  }
};

class KangarooRecoveryLeaseGuard {
public:
  static KangarooRecoveryLeaseResult verify(
      const std::string &serverUrl,
      const client::ClientExecutionState &state);
};

} // namespace openpuzzle
