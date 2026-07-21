#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/runtime/ExecutionProgress.hpp"

#include <optional>
#include <string>

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

  bool hasProgress = false;
  ExecutionProgress progress;

  bool progressUploaded = false;

  AssignmentUploadStatus progressStatus =
      AssignmentUploadStatus::NotAttempted;

  std::string progressError;

  bool hasExitCode = false;
  int exitCode = 0;

  bool completionUploaded = false;

  AssignmentUploadStatus completionStatus =
      AssignmentUploadStatus::NotAttempted;

  bool stateRemoved = false;
  std::string completionError;
};

class ExecutionSyncService {
public:
  ExecutionSyncResult tick(
      const std::string& serverUrl) const;

  static std::optional<ExecutionProgress>
  latestProgress(
      const std::string& workspace);

  static std::optional<std::string>
  solutionFile(
      const std::string& workspace);

  static bool hasCompletionProof(
      const std::string& workspace);

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
  static bool processExists(
      int pid);

  static bool readExitCode(
      const std::string& workspace,
      int& exitCode);

  static bool readLatestProgress(
      const std::string& workspace,
      ExecutionProgress& progress);
};

} // namespace openpuzzle::client
