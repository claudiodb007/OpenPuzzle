#include "openpuzzle/client/ExecutionSyncService.hpp"

#include "openpuzzle/adapters/bitcrack/BitCrackProgressParser.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"

#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>
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
      logPath);

  if (!input) {
    return false;
  }

  bitcrack::BitCrackProgressParser parser;

  bool found = false;

  std::string line;

  while (std::getline(
      input,
      line)) {
    const auto parsed =
        parser.parseLine(line);

    if (!parsed) {
      continue;
    }

    /*
     * Apenas métricas públicas de progresso.
     *
     * Eventos Found, mensagens de erro e qualquer
     * conteúdo potencialmente sensível são ignorados.
     */
    if (parsed->speedMKeys > 0.0 &&
        !parsed->keysChecked.empty()) {
      progress =
          *parsed;

      found = true;
    }
  }

  return found;
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

    if (!result.progressUploaded) {
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

  const std::string finalStatus =
      exitCode == 0
          ? "completed"
          : "failed";

  HttpRangeClient httpClient(
      serverUrl);

  result.completionUploaded =
      httpClient.complete(
          state->assignmentId,
          state->clientId,
          exitCode,
          finalStatus);

  if (!result.completionUploaded) {
    result.completionError =
        httpClient.lastError();

    return result;
  }

  result.stateRemoved =
      ClientStateStore::remove();

  if (!result.stateRemoved) {
    result.completionError =
        "Unable to remove local execution state";
  }

  return result;
}

} // namespace openpuzzle::client
