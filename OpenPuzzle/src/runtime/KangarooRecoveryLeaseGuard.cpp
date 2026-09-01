#include "openpuzzle/runtime/KangarooRecoveryLeaseGuard.hpp"

#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace openpuzzle {
namespace {

std::string lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });
  return value;
}

KangarooRecoveryLeaseResult result(
    KangarooRecoveryLeaseStatus status,
    std::string error = {},
    double speedMKeys = 0.0) {
  KangarooRecoveryLeaseResult value;
  value.status = status;
  value.error = std::move(error);
  value.speedMKeys = speedMKeys;
  return value;
}

} // namespace

KangarooRecoveryLeaseResult KangarooRecoveryLeaseGuard::verify(
    const std::string &serverUrl,
    const client::ClientExecutionState &state) {
  const auto engine = lower(state.engine);
  if (!state.valid() ||
      (engine != "kangaroo" && engine != "psckangaroo")) {
    return result(
        KangarooRecoveryLeaseStatus::Invalid,
        "Invalid Kangaroo recovery state");
  }

  const auto progress =
      client::ExecutionSyncService::latestProgress(
          state.workspace,
          state.engine);

  if (!progress || progress->speedMKeys <= 0.0 ||
      progress->keysChecked != "0") {
    return result(
        KangarooRecoveryLeaseStatus::Invalid,
        "Kangaroo recovery has no valid telemetry proof");
  }

  client::HttpRangeClient http(serverUrl);
  if (http.progress(
          state.assignmentId,
          state.clientId,
          progress->speedMKeys,
          "0")) {
    return result(
        KangarooRecoveryLeaseStatus::Accepted,
        {},
        progress->speedMKeys);
  }

  const auto classification =
      client::ExecutionSyncService::classifyProgressError(
          http.lastErrorCode());

  if (classification ==
      client::AssignmentUploadStatus::AssignmentRejected) {
    return result(
        KangarooRecoveryLeaseStatus::Rejected,
        http.lastError(),
        progress->speedMKeys);
  }

  if (classification ==
      client::AssignmentUploadStatus::PermanentFailure) {
    return result(
        KangarooRecoveryLeaseStatus::Invalid,
        http.lastError(),
        progress->speedMKeys);
  }

  return result(
      KangarooRecoveryLeaseStatus::Retry,
      http.lastError(),
      progress->speedMKeys);
}

} // namespace openpuzzle
