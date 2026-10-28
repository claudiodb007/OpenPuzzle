#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/runtime/ExecutionProgress.hpp"

#include <optional>
#include <string>
#include <vector>

namespace openpuzzle::client {

enum class AssignmentUploadStatus {
  NotAttempted,
  Uploaded,
  TemporaryFailure,
  AssignmentRejected,
  PermanentFailure
};

struct ExecutionSyncResult {
  bool hasState = false;

  ClientExecutionState state;

  bool solutionFound = false;
  std::string solutionPath;

  bool running = false;
  bool identityUnavailable = false;

  /*
   * O processo desapareceu sem produzir exit.code.
   * Esta situação nunca representa conclusão.
   */
  bool interrupted = false;
  bool checkpointPreserved = false;

  bool hasProgress = false;
  ExecutionProgress progress;

  bool progressUploaded = false;

  AssignmentUploadStatus progressStatus =
      AssignmentUploadStatus::NotAttempted;

  std::string progressError;
  std::string progressReason;

  bool hasExitCode = false;
  int exitCode = 0;
  // Full native DWORD remains available even when the server's signed integer
  // contract requires mapping a large nonzero Windows value to failure -4.
  std::optional<std::uint32_t> nativeExitCode;

  bool completionUploaded = false;

  AssignmentUploadStatus completionStatus =
      AssignmentUploadStatus::NotAttempted;

  bool stateRemoved = false;
  std::string completionError;

  bool calibrationAttempted = false;
  bool calibrationUpdated = false;
  double calibratedPlanningSpeed = 0.0;
  std::size_t calibrationSamples = 0;
  std::string calibrationError;
};

class ExecutionSyncService {
public:
  using ProcessStartTimeReader =
      std::optional<std::uint64_t> (*)(int);

  ExecutionSyncService();

  // Allows deterministic testing of temporary /proc read failures.
  explicit ExecutionSyncService(
      ProcessStartTimeReader processStartTimeReader);

  ExecutionSyncResult inspect() const;

  ExecutionSyncResult inspect(
      const std::string& executionSlot) const;

  ExecutionSyncResult tick(
      const std::string& serverUrl) const;

  ExecutionSyncResult tick(
      const std::string& serverUrl,
      const std::string& executionSlot) const;

  static std::optional<ExecutionProgress>
  latestProgress(
      const std::string& workspace,
      const std::string& engine = {});

  static std::vector<double>
  speedSamples(
      const std::string& workspace,
      const std::string& engine = {});

  static std::optional<std::string>
  solutionFile(
      const std::string& workspace,
      const std::string& engine = {});

  static bool hasCompletionProof(
      const std::string& workspace,
      const std::string& engine = {},
      const std::string& expectedKeys = {});

  static std::optional<std::string>
  assignedKeyCount(
      const std::string& start,
      const std::string& end);

  static AssignmentUploadStatus
  classifyProgressError(
      const std::string& errorCode);

  static AssignmentUploadStatus
  classifyCompletionError(
      const std::string& errorCode);

private:
  enum class ProcessIdentityStatus {
    Running,
    Stopped,
    Unknown
  };

  ProcessStartTimeReader processStartTimeReader_;

  ProcessIdentityStatus processIdentityStatus(
      const ClientExecutionState& state) const;

  static bool readExitCode(
      const ClientExecutionState& state,
      int& exitCode);

  static bool readLatestProgress(
      const std::string& workspace,
      const std::string& engine,
      ExecutionProgress& progress);
};

} // namespace openpuzzle::client
