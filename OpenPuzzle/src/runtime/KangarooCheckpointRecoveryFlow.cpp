#include "openpuzzle/runtime/KangarooCheckpointRecoveryFlow.hpp"

#include "openpuzzle/runtime/KangarooCheckpointRecoveryCoordinator.hpp"
#include "openpuzzle/runtime/KangarooCheckpointRecoveryPlanner.hpp"
#include "openpuzzle/runtime/KangarooRecoveryLeaseGuard.hpp"

#include <string>
#include <utility>

namespace openpuzzle {
namespace {

KangarooCheckpointRecoveryFlowResult result(
    KangarooCheckpointRecoveryFlowStatus status,
    std::string error = {}) {
  KangarooCheckpointRecoveryFlowResult value;
  value.status = status;
  value.error = std::move(error);
  return value;
}

} // namespace

KangarooCheckpointRecoveryFlowResult
KangarooCheckpointRecoveryFlow::recover(
    const std::string &serverUrl,
    const client::ClientExecutionState &state,
    const std::filesystem::path &executable,
    const std::filesystem::path &expectedWorkspace) {
  const auto plan =
      KangarooCheckpointRecoveryPlanner::build(
          state,
          executable,
          expectedWorkspace);

  if (!plan.eligible) {
    return result(
        KangarooCheckpointRecoveryFlowStatus::Invalid,
        plan.error);
  }

  const auto lease =
      KangarooRecoveryLeaseGuard::verify(
          serverUrl,
          state);

  switch (lease.status) {
  case KangarooRecoveryLeaseStatus::Retry:
    return result(
        KangarooCheckpointRecoveryFlowStatus::Retry,
        lease.error);

  case KangarooRecoveryLeaseStatus::Rejected:
    return result(
        KangarooCheckpointRecoveryFlowStatus::Rejected,
        lease.error);

  case KangarooRecoveryLeaseStatus::Invalid:
    return result(
        KangarooCheckpointRecoveryFlowStatus::Invalid,
        lease.error);

  case KangarooRecoveryLeaseStatus::Accepted:
    break;
  }

  const auto recovery =
      KangarooCheckpointRecoveryCoordinator::resume(
          state,
          plan,
          lease);

  if (!recovery.resumed) {
    return result(
        KangarooCheckpointRecoveryFlowStatus::Invalid,
        recovery.error);
  }

  KangarooCheckpointRecoveryFlowResult value;
  value.status = KangarooCheckpointRecoveryFlowStatus::Resumed;
  value.state = recovery.state;
  return value;
}

} // namespace openpuzzle
