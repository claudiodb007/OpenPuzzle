#include "openpuzzle/client/ExecutionSyncService.hpp"

#include "openpuzzle/adapters/bitcrack/BitCrackProgressParser.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <cerrno>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace openpuzzle::client {

bool ExecutionSyncService::processExists(
    int pid) {
  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
}

bool ExecutionSyncService::readExitCode(
    const std::string& workspace,
    int& exitCode) {
  const auto exitPath =
      std::filesystem::path(workspace) /
      "exit.code";

  std::ifstream input(
      exitPath);

  if (!input) {
    return false;
  }

  int value = 0;

  if (!(input >> value)) {
    return false;
  }

  exitCode = value;

  return true;
}

bool ExecutionSyncService::readLatestProgress(
    const std::string& workspace,
    ExecutionProgress& progress) {
  const auto logPath =
      std::filesystem::path(workspace) /
      "bitcrack.log";

  std::ifstream input(
      logPath,
      std::ios::binary);

  if (!input) {
    return false;
  }

  bitcrack::BitCrackProgressParser parser;

  bool found = false;

  const auto parseRecord =
      [&](const std::string& record) {
        if (record.empty()) {
          return;
        }

        const auto parsed =
            parser.parseLine(record);

        if (!parsed) {
          return;
        }

        /*
         * Apenas métricas públicas de progresso.
         *
         * Eventos Found, mensagens de erro e qualquer
         * conteúdo potencialmente sensível são
         * ignorados.
         */
        if (parsed->speedMKeys > 0.0 &&
            !parsed->keysChecked.empty()) {
          progress =
              *parsed;

          found = true;
        }
      };

  /*
   * BitCrack atualiza a mesma linha do terminal
   * usando carriage return. Os logs podem, por isso,
   * conter registos separados por CR, LF ou CRLF.
   */
  std::string record;
  char character = '\0';

  while (input.get(character)) {
    if (
        character == '\r' ||
        character == '\n') {
      parseRecord(record);
      record.clear();
      continue;
    }

    record.push_back(character);
  }

  parseRecord(record);

  return found;
}

std::optional<ExecutionProgress>
ExecutionSyncService::latestProgress(
    const std::string& workspace) {
  ExecutionProgress progress;

  if (!readLatestProgress(
          workspace,
          progress)) {
    return std::nullopt;
  }

  return progress;
}

bool ExecutionSyncService::hasCompletionProof(
    const std::string& workspace) {
  if (!latestProgress(workspace)) {
    return false;
  }

  const auto logPath =
      std::filesystem::path(workspace) /
      "bitcrack.log";

  std::ifstream input(
      logPath,
      std::ios::binary);

  if (!input) {
    return false;
  }

  std::string line;

  while (std::getline(input, line)) {
    if (
        line.find(
            "Reached end of keyspace") !=
        std::string::npos) {
      return true;
    }
  }

  return false;
}

std::optional<std::string>
ExecutionSyncService::assignedKeyCount(
    const std::string& start,
    const std::string& end) {
  using boost::multiprecision::cpp_int;

  const auto parseHex =
      [](const std::string& value)
          -> std::optional<cpp_int> {
        if (value.empty()) {
          return std::nullopt;
        }

        cpp_int result = 0;

        for (const unsigned char character : value) {
          unsigned int digit = 0;

          if (character >= '0' && character <= '9') {
            digit = character - '0';
          } else if (
              character >= 'a' && character <= 'f') {
            digit = character - 'a' + 10;
          } else if (
              character >= 'A' && character <= 'F') {
            digit = character - 'A' + 10;
          } else {
            return std::nullopt;
          }

          result <<= 4;
          result += digit;
        }

        return result;
      };

  const auto startValue = parseHex(start);
  const auto endValue = parseHex(end);

  if (
      !startValue ||
      !endValue ||
      *endValue < *startValue) {
    return std::nullopt;
  }

  const cpp_int count =
      *endValue - *startValue + 1;

  return count.str();
}

std::optional<std::string>
ExecutionSyncService::solutionFile(
    const std::string& workspace) {
  const auto path =
      std::filesystem::path(workspace) /
      "found.txt";

  std::error_code error;

  if (
      !std::filesystem::is_regular_file(
          path,
          error) ||
      error) {
    return std::nullopt;
  }

  const auto size =
      std::filesystem::file_size(
          path,
          error);

  if (error || size == 0) {
    return std::nullopt;
  }

  return path.string();
}

AssignmentUploadStatus
ExecutionSyncService::classifyProgressError(
    const std::string& errorCode) {
  if (
      errorCode == "assignment_not_found" ||
      errorCode == "assignment_client_mismatch" ||
      errorCode == "invalid_assignment_state" ||
      errorCode == "assignment_lease_expired") {
    return
        AssignmentUploadStatus::
            AssignmentRejected;
  }

  if (
      errorCode == "method_not_allowed" ||
      errorCode == "invalid_json" ||
      errorCode == "invalid_request" ||
      errorCode == "invalid_assignment_id" ||
      errorCode == "invalid_client_id" ||
      errorCode == "invalid_status" ||
      errorCode == "invalid_speed" ||
      errorCode == "invalid_keys_checked") {
    return
        AssignmentUploadStatus::
            PermanentFailure;
  }

  return
      AssignmentUploadStatus::
          TemporaryFailure;
}

AssignmentUploadStatus
ExecutionSyncService::classifyCompletionError(
    const std::string& errorCode) {
  if (
      errorCode == "assignment_not_found" ||
      errorCode == "assignment_client_mismatch" ||
      errorCode == "invalid_assignment_state") {
    return
        AssignmentUploadStatus::
            AssignmentRejected;
  }

  if (
      errorCode == "method_not_allowed" ||
      errorCode == "invalid_json" ||
      errorCode == "invalid_request" ||
      errorCode == "invalid_assignment_id" ||
      errorCode == "invalid_client_id" ||
      errorCode == "invalid_exit_code" ||
      errorCode == "invalid_status" ||
      errorCode == "invalid_completed_exit_code" ||
      errorCode == "invalid_final_exit_code" ||
      errorCode == "invalid_keys_checked" ||
      errorCode == "incomplete_assignment_coverage") {
    return
        AssignmentUploadStatus::
            PermanentFailure;
  }

  return
      AssignmentUploadStatus::
          TemporaryFailure;
}

ExecutionSyncResult
ExecutionSyncService::tick(
    const std::string& serverUrl) const {
  ExecutionSyncResult result;

  const auto state =
      ClientStateStore::load();

  if (!state) {
    return result;
  }

  result.hasState = true;
  result.state = *state;

  result.running =
      processExists(
          state->pid);

  const auto detectedSolution =
      solutionFile(
          state->workspace);

  if (detectedSolution) {
    result.solutionFound = true;
    result.solutionPath =
        *detectedSolution;

    /*
     * Não finalizar, remover estado, ler nem
     * transmitir o conteúdo de found.txt.
     */
    return result;
  }

  if (result.running) {
    ExecutionProgress progress;

    if (!readLatestProgress(
            state->workspace,
            progress)) {
      return result;
    }

    result.hasProgress = true;
    result.progress = progress;

    HttpRangeClient httpClient(
        serverUrl);

    result.progressUploaded =
        httpClient.progress(
            state->assignmentId,
            state->clientId,
            progress.speedMKeys,
            progress.keysChecked);

    if (result.progressUploaded) {
      result.progressStatus =
          AssignmentUploadStatus::Uploaded;
    } else {
      result.progressStatus =
          classifyProgressError(
              httpClient.lastErrorCode());

      result.progressError =
          httpClient.lastError();
    }

    return result;
  }

  int exitCode = 0;

  if (!readExitCode(
          state->workspace,
          exitCode)) {
    return result;
  }

  result.hasExitCode = true;
  result.exitCode = exitCode;

  const auto finalProgress =
      latestProgress(
          state->workspace);

  const bool completed =
      exitCode == 0;

  std::string finalKeysChecked =
      finalProgress
          ? finalProgress->keysChecked
          : "";

  if (completed) {
    if (!hasCompletionProof(state->workspace)) {
      result.completionStatus =
          AssignmentUploadStatus::PermanentFailure;

      result.completionError =
          "BitCrack did not provide complete-range proof; "
          "local state was preserved";

      return result;
    }

    const auto assignedKeys =
        assignedKeyCount(
            state->start,
            state->end);

    if (!assignedKeys) {
      result.completionStatus =
          AssignmentUploadStatus::PermanentFailure;

      result.completionError =
          "Assigned keyspace is invalid; "
          "local state was preserved";

      return result;
    }

    /*
     * A última linha periódica do BitCrack pode ser
     * anterior ao fim por alguns segundos. Depois do
     * marcador de fim, o tamanho exato do assignment
     * é a contagem final comprovada.
     */
    finalKeysChecked =
        *assignedKeys;
  }

  const std::string finalStatus =
      completed
          ? "completed"
          : "failed";

  HttpRangeClient httpClient(
      serverUrl);

  result.completionUploaded =
      httpClient.complete(
          state->assignmentId,
          state->clientId,
          exitCode,
          finalStatus,
          finalKeysChecked);

  if (!result.completionUploaded) {
    result.completionStatus =
        classifyCompletionError(
            httpClient.lastErrorCode());

    result.completionError =
        httpClient.lastError();

    if (
        result.completionStatus ==
        AssignmentUploadStatus::
            AssignmentRejected) {
      result.stateRemoved =
          ClientStateStore::remove();

      if (!result.stateRemoved) {
        result.completionError +=
            "; Unable to remove local "
            "execution state";
      }
    }

    return result;
  }

  result.completionStatus =
      AssignmentUploadStatus::Uploaded;

  result.stateRemoved =
      ClientStateStore::remove();

  if (!result.stateRemoved) {
    result.completionError =
        "Unable to remove local execution state";
  }

  return result;
}

} // namespace openpuzzle::client
