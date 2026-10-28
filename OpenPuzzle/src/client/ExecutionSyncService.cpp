#include "openpuzzle/client/ExecutionSyncService.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/engines/EngineParserFactory.hpp"
#ifdef OPENPUZZLE_ENABLE_VANITY_PREVIEW
#include "VanityCommonEngine.hpp"
#endif
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#ifdef _WIN32
#include "openpuzzle/runtime/WindowsExecutionState.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include "openpuzzle/runtime/WindowsEngineProcess.hpp"
#else
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/performance/AdaptiveProfileUpdater.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#endif

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <limits>
#include <memory>

namespace openpuzzle::client {
namespace {
std::optional<std::uint64_t> defaultStartTime(int pid) {
#ifdef _WIN32
  return WindowsProcessIdentity::creationTime(static_cast<std::uint32_t>(pid));
#else
  return LinuxProcessIdentity::startTime(pid);
#endif
}
std::string artifacts(const ClientExecutionState& state) {
#ifdef _WIN32
  const auto record = windowsExecutionRecord(state);
  return record ? pathUtf8(WindowsEngineProcess::generationPath(pathFromUtf8(state.workspace), *record)) : std::string{};
#else
  return state.workspace;
#endif
}
std::string engineLog(const std::string& legacy) {
#ifdef _WIN32
  (void)legacy;
  return "engine.log";
#else
  return legacy;
#endif
}
void nativeExit(const ClientExecutionState& state, ExecutionSyncResult& result) {
#ifdef _WIN32
  const auto expected = windowsExecutionRecord(state);
  const auto recovered = WindowsEngineProcess::recover(pathFromUtf8(state.workspace));
  if (expected && recovered.record == expected) result.nativeExitCode = recovered.exitCode;
#else
  (void)state; (void)result;
#endif
}
}


ExecutionSyncService::ExecutionSyncService()
    : ExecutionSyncService(
          defaultStartTime) {}

ExecutionSyncService::ExecutionSyncService(
    ProcessStartTimeReader processStartTimeReader)
    : processStartTimeReader_(
          processStartTimeReader
              ? processStartTimeReader
              : defaultStartTime) {}

ExecutionSyncService::ProcessIdentityStatus
ExecutionSyncService::processIdentityStatus(
    const ClientExecutionState& state) const {
#ifdef _WIN32
  const auto expected = windowsExecutionRecord(state);
  if (!expected) return ProcessIdentityStatus::Unknown;
  const auto recovered = WindowsEngineProcess::recover(pathFromUtf8(state.workspace));
  if (recovered.record != expected || recovered.completionUnavailable ||
      recovered.status == WindowsProcessIdentity::Status::Unavailable)
    return ProcessIdentityStatus::Unknown;
  if (recovered.status == WindowsProcessIdentity::Status::Inactive) {
    if (!recovered.exitCode && WindowsProcessIdentity::status(state.ownerPid, state.ownerStartTime) !=
        WindowsProcessIdentity::Status::Inactive)
      return ProcessIdentityStatus::Unknown; // The owner may still be flushing/publishing exit.
    return ProcessIdentityStatus::Stopped;
  }
  const auto actual = processStartTimeReader_(state.pid);
  if (!actual) return ProcessIdentityStatus::Unknown;
  return *actual == state.processStartTime ? ProcessIdentityStatus::Running : ProcessIdentityStatus::Stopped;
#else
  if (!state.processPlatform.empty()) return ProcessIdentityStatus::Unknown;
  /*
   * OpenPuzzle <= 1.0.16 has no boot_id and OpenPuzzle <= 1.0.17 has no
   * process_start_time.
   *
   * Both are incomplete identities. Fail closed so legacy execution state
   * enters recovery rather than trusting a recycled numeric PID.
   */
  if (state.bootId.empty() ||
      state.processStartTime == 0 ||
      state.pid <= 0) {
    return ProcessIdentityStatus::Stopped;
  }

  const auto currentBootId =
      ClientStateStore::currentBootId();

  if (currentBootId.empty()) {
    return ProcessIdentityStatus::Unknown;
  }

  if (state.bootId != currentBootId) {
    return ProcessIdentityStatus::Stopped;
  }

  if (kill(state.pid, 0) != 0) {
    if (errno == ESRCH) {
      return ProcessIdentityStatus::Stopped;
    }

    if (errno != EPERM) {
      return ProcessIdentityStatus::Unknown;
    }
  }

  const auto currentStartTime =
      processStartTimeReader_(state.pid);

  if (!currentStartTime) {
    // The process may have exited between kill(0) and the /proc read.
    // Only ESRCH proves absence; a denied/unreadable stat is still uncertain.
    if (kill(state.pid, 0) != 0 && errno == ESRCH) {
      return ProcessIdentityStatus::Stopped;
    }

    return ProcessIdentityStatus::Unknown;
  }

  return *currentStartTime == state.processStartTime
             ? ProcessIdentityStatus::Running
             : ProcessIdentityStatus::Stopped;
#endif
}

bool ExecutionSyncService::readExitCode(
    const ClientExecutionState& state,
    int& exitCode) {
#ifdef _WIN32
  const auto expected = windowsExecutionRecord(state);
  const auto recovered = WindowsEngineProcess::recover(pathFromUtf8(state.workspace));
  if (!expected || recovered.record != expected || !recovered.exitCode ||
      recovered.status != WindowsProcessIdentity::Status::Inactive) return false;
  const auto code = *recovered.exitCode;
  exitCode = code <= static_cast<std::uint32_t>((std::numeric_limits<int>::max)())
      ? static_cast<int>(code) : -4;
  return true;
#else
  const auto& workspace = state.workspace;
  const auto exitPath =
      pathFromUtf8(workspace) /
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
#endif
}

bool ExecutionSyncService::readLatestProgress(
    const std::string& workspace,
    const std::string& engine,
    ExecutionProgress& progress) {
  std::string engineId = engine;

  std::transform(
      engineId.begin(),
      engineId.end(),
      engineId.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  if (engineId.empty()) {
    engineId = "bitcrack";
  }

  const std::string logName =
      engineId == "vanity" ? "vanity.log" : engineId == "keyhunt"
          ? "keyhunt.log"
          : (
                engineId == "kangaroo" ||
                        engineId == "psckangaroo"
                    ? "kangaroo.log"
                    : "bitcrack.log"
            );

  const auto logPath =
      pathFromUtf8(workspace) /
      engineLog(logName);

  std::ifstream input(
      logPath,
      std::ios::binary);

  if (!input) {
    return false;
  }

  auto parser =
      EngineParserFactory::create(
          engineId);

  if (!parser) {
    return false;
  }

  bool found = false;

  const auto parseRecord =
      [&](const std::string& record) {
        if (record.empty()) {
          return;
        }

        const auto parsed =
            parser->parseLine(record);

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

#ifdef OPENPUZZLE_ENABLE_VANITY_PREVIEW
  if(engineId=="vanity"&&found&&!vanityCommonProgressWithinCoverage(workspace,progress.keysChecked))return false;
#endif
  return found;
}

std::optional<ExecutionProgress>
ExecutionSyncService::latestProgress(
    const std::string& workspace,
    const std::string& engine) {
  if (engine.empty()) {
    const auto bitcrack =
        latestProgress(
            workspace,
            "BitCrack");

    if (bitcrack) {
      return bitcrack;
    }

    return latestProgress(
        workspace,
        "KeyHunt");
  }

  ExecutionProgress progress;

  if (!readLatestProgress(
          workspace,
          engine,
          progress)) {
    return std::nullopt;
  }

  return progress;
}

bool ExecutionSyncService::hasCompletionProof(
    const std::string& workspace,
    const std::string& engine,
    const std::string& expectedKeys) {
  if (engine.empty()) {
    return
        hasCompletionProof(
            workspace,
            "BitCrack", expectedKeys) ||
        hasCompletionProof(
            workspace,
            "KeyHunt", expectedKeys);
  }

  /*
   * O marcador final do motor é independente das
   * amostras periódicas. Um range curto pode terminar
   * antes de BitCrack imprimir MKey/s e total.
   *
   * tick() só usa esta prova depois de confirmar
   * exit.code=0 e calcula a contagem final diretamente
   * a partir do range atribuído.
   */
  std::string engineId = engine;

  std::transform(
      engineId.begin(),
      engineId.end(),
      engineId.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  const bool keyhunt =
      engineId == "keyhunt";

#ifdef OPENPUZZLE_ENABLE_VANITY_PREVIEW
  if(engineId=="vanity")return vanityCommonCompletionProof(workspace,expectedKeys);
#else
  if(engineId=="vanity")return false;
#endif

  const bool kangaroo =
      engineId == "kangaroo" ||
      engineId == "psckangaroo";

  const auto logPath =
      pathFromUtf8(workspace) /
      engineLog(
          keyhunt
              ? "keyhunt.log"
              : (
                    kangaroo
                        ? "kangaroo.log"
                        : "bitcrack.log"
                )
      );

  std::ifstream input(
      logPath,
      std::ios::binary);

  if (!input) {
    return false;
  }

  const auto provesCompletion =
      [keyhunt, kangaroo, &expectedKeys](const std::string& record) {
        if (kangaroo) {
          /*
           * Pollard Kangaroo is probabilistic. A clean process exit or a
           * checkpoint message is not deterministic proof that the assigned
           * interval was exhausted.
           */
          return false;
        }

        if (keyhunt) {
#ifdef _WIN32
          const auto parsed=EngineParserFactory::create("keyhunt")->parseLine(record);
          return parsed&&parsed->finished&&!parsed->keysChecked.empty()&&
              (expectedKeys.empty()||parsed->keysChecked==expectedKeys);
#else
          return record == "End";
#endif
        }

        return
            record.find(
                "Reached end of keyspace") !=
            std::string::npos;
      };

  std::string record;
  char character = '\0';

  while (input.get(character)) {
    if (
        character == '\r' ||
        character == '\n') {
      if (provesCompletion(record)) {
        return true;
      }

      record.clear();
      continue;
    }

    record.push_back(character);
  }

  return provesCompletion(record);
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


std::vector<double>
ExecutionSyncService::speedSamples(
    const std::string& workspace,
    const std::string& engine) {
  std::string engineId = engine;

  std::transform(
      engineId.begin(),
      engineId.end(),
      engineId.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  if (engineId.empty()) {
    engineId = "bitcrack";
  }

  const auto logPath =
      pathFromUtf8(workspace) /
      engineLog(
          engineId == "vanity" ? "vanity.log" : engineId == "keyhunt"
              ? "keyhunt.log"
              : (
                    engineId == "kangaroo" ||
                            engineId == "psckangaroo"
                        ? "kangaroo.log"
                        : "bitcrack.log"
                )
      );

  std::ifstream input(
      logPath,
      std::ios::binary);

  if (!input) {
    return {};
  }

  auto parser =
      EngineParserFactory::create(
          engineId);

  if (!parser) {
    return {};
  }

  std::vector<double> samples;

  const auto parseRecord =
      [&](const std::string& record) {
        if (record.empty()) {
          return;
        }

        const auto parsed =
            parser->parseLine(record);

        if (parsed &&
            parsed->speedMKeys > 0.0 &&
            !parsed->keysChecked.empty()) {
          samples.push_back(
              parsed->speedMKeys);
        }
      };

  std::string record;
  char character = '\0';

  while (input.get(character)) {
    if (character == '\r' ||
        character == '\n') {
      parseRecord(record);
      record.clear();
      continue;
    }

    record.push_back(character);
  }

  parseRecord(record);
  return samples;
}

std::optional<std::string>
ExecutionSyncService::solutionFile(
    const std::string& workspace,
    const std::string& engine) {
  const auto root =
      pathFromUtf8(workspace);

  const auto nonEmptyDirectFile =
      [](const std::filesystem::path& path) {
        std::error_code error;
        const auto status =
            std::filesystem::symlink_status(
                path,
                error);

        if (error ||
            std::filesystem::is_symlink(status) ||
            !std::filesystem::is_regular_file(status)) {
          return false;
        }

        const auto size =
            std::filesystem::file_size(
                path,
                error);

        return !error && size > 0;
      };

  const auto found =
      root / "found.txt";

  if (nonEmptyDirectFile(found)) {
    return pathUtf8(found);
  }

  std::string normalizedEngine = engine;

  std::transform(
      normalizedEngine.begin(),
      normalizedEngine.end(),
      normalizedEngine.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  if (normalizedEngine == "kangaroo" ||
      normalizedEngine == "psckangaroo") {
    const auto nativeResult =
        root / "RESULTS.TXT";

    if (nonEmptyDirectFile(nativeResult)) {
      return pathUtf8(nativeResult);
    }
  }

#ifdef _WIN32
  if (normalizedEngine == "keyhunt") {
    const auto nativeResult = root / "KEYFOUNDKEYFOUND.txt";
    if (nonEmptyDirectFile(nativeResult)) return pathUtf8(nativeResult);
  }
#endif
  return std::nullopt;
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
ExecutionSyncService::inspect() const {
  return inspect(
      ClientStateStore::executionSlot());
}

ExecutionSyncResult
ExecutionSyncService::inspect(
    const std::string& executionSlot) const {
  ExecutionSyncResult result;

  const auto state =
      ClientStateStore::load(
          executionSlot);

  if (!state) {
    return result;
  }

  result.hasState = true;
  result.state = *state;
  const auto processStatus =
      processIdentityStatus(*state);
#ifdef _WIN32
  if (processStatus == ProcessIdentityStatus::Unknown) {
    result.identityUnavailable = true;
    result.completionError = "Windows execution identity or generation is unavailable; local state was preserved";
    return result;
  }
#endif
  nativeExit(*state, result);
  result.running =
      processStatus == ProcessIdentityStatus::Running;
  result.identityUnavailable =
      processStatus == ProcessIdentityStatus::Unknown;

  const auto detectedSolution =
      solutionFile(
          state->workspace,
          state->engine);

  if (detectedSolution) {
    result.solutionFound = true;
    result.solutionPath =
        *detectedSolution;
    return result;
  }

  if (result.running) {
    ExecutionProgress progress;

    if (readLatestProgress(
            artifacts(*state),
            state->engine,
            progress)) {
      result.hasProgress = true;
      result.progress = progress;
    }

    return result;
  }

  int exitCode = 0;
  if (readExitCode(
          *state,
          exitCode)) {
    result.hasExitCode = true;
    result.exitCode = exitCode;
  } else {
    result.interrupted =
        processStatus == ProcessIdentityStatus::Stopped;
  }

  return result;
}

ExecutionSyncResult
ExecutionSyncService::tick(
    const std::string& serverUrl) const {
  return tick(
      serverUrl,
      ClientStateStore::executionSlot());
}

ExecutionSyncResult
ExecutionSyncService::tick(
    const std::string& serverUrl,
    const std::string& executionSlot) const {
  ExecutionSyncResult result;
#ifdef _WIN32
  std::unique_ptr<windowsFile::Lock> lock;
  try {
    const auto statePath = ClientStateStore::path(executionSlot);
    WorkspaceSecurity::prepare(statePath.parent_path());
    lock = std::make_unique<windowsFile::Lock>(statePath.wstring() + L".execution.lock");
  } catch (const std::exception& error) {
    result.identityUnavailable = true;
    result.completionError = std::string("Windows synchronization storage is unavailable; state was preserved: ") + error.what();
    return result;
  }
  if (!lock->locked()) {
    result.identityUnavailable = true;
    result.completionError = "Windows assignment synchronization is already in progress";
    return result;
  }
#endif


  const auto state =
      ClientStateStore::load(
          executionSlot);

  if (!state) {
    return result;
  }

  result.hasState = true;
  result.state = *state;

  const auto processStatus =
      processIdentityStatus(*state);
#ifdef _WIN32
  if (processStatus == ProcessIdentityStatus::Unknown) {
    result.identityUnavailable = true;
    result.completionError = "Windows execution identity or generation is unavailable; local state was preserved";
    return result;
  }
#endif
  nativeExit(*state, result);
  result.running =
      processStatus == ProcessIdentityStatus::Running;
  result.identityUnavailable =
      processStatus == ProcessIdentityStatus::Unknown;

  const auto detectedSolution =
      solutionFile(
          state->workspace,
          state->engine);

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
            artifacts(*state),
            state->engine,
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

      result.progressReason =
          HttpRangeClient::parseErrorReason(
              result.progressError);
    }

    return result;
  }

  if (processStatus == ProcessIdentityStatus::Unknown) {
    result.completionError =
        "Process identity temporarily unavailable; "
        "local state was preserved";
    return result;
  }

  int exitCode = 0;

  /*
   * O launcher escreve exit.code antes de o processo
   * supervisor terminar. Se o processo já desapareceu
   * e esse ficheiro não existe, a execução foi
   * interrompida abruptamente, por exemplo por falha
   * elétrica ou reinício do sistema.
   *
   * Nunca considerar este caso como conclusão.
   */
  if (!readExitCode(
          *state,
          exitCode)) {
#ifdef _WIN32
    // Revalidate the generation after reading completion: a concurrent marker
    // replacement or an unavailable record cannot authorize cancellation.
    if (processIdentityStatus(*state) != ProcessIdentityStatus::Stopped) {
      result.identityUnavailable = true;
      result.completionError = "Windows completion identity changed; local state was preserved";
      return result;
    }
#endif
    result.interrupted = true;

    // A live OpenPuzzle runtime is not evidence that its engine is alive.
    // Definitively stale identities must synchronize interruption even
    // when this very runtime owns the slot and is performing recovery.
    exitCode = -3;
  }

  result.hasExitCode = true;
  result.exitCode = exitCode;

  const auto finalProgress =
      latestProgress(
          artifacts(*state),
          state->engine);

  const bool completed =
      exitCode == 0;

  std::string finalKeysChecked =
      finalProgress
          ? finalProgress->keysChecked
          : "";

  if (completed) {
    if (!hasCompletionProof(
            artifacts(*state),
            state->engine,
            assignedKeyCount(state->start,state->end).value_or(""))) {
      result.completionStatus =
          AssignmentUploadStatus::PermanentFailure;

      result.completionError =
          state->engine +
          " did not provide complete-range proof; "
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
     * A última linha periódica do motor pode ser
     * anterior ao fim. Depois do marcador específico
     * do motor, o tamanho exato do assignment é a
     * contagem final comprovada.
     */
    finalKeysChecked =
        *assignedKeys;
  }

  const std::string finalStatus =
      completed
          ? "completed"
          : (
                result.interrupted
                    ? "cancelled"
                    : "failed"
            );

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
          ClientStateStore::remove(
          executionSlot);

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

  if (completed && state->profileManaged) {
#ifdef _WIN32
    result.calibrationError = "Adaptive profile calibration is not yet available on Windows";
#else
    result.calibrationAttempted = true;

    const char* home =
        std::getenv("HOME");

    if (home == nullptr) {
      result.calibrationError =
          "HOME is not set";
    } else {
      Database database;

      const std::string databasePath =
          std::string(home) +
          "/.local/share/OpenPuzzle/openpuzzle.db";

      if (!database.open(databasePath) ||
          !database.createSchema()) {
        result.calibrationError =
            "Unable to open the local profile database";
      } else {
        performance::AdaptiveProfileUpdater updater(
            database);

        const auto update = updater.update(
            *state,
            speedSamples(
                state->workspace,
                state->engine));

        result.calibrationUpdated =
            update.updated;
        result.calibratedPlanningSpeed =
            update.calibratedPlanningSpeed;
        result.calibrationSamples =
            update.acceptedSamples;
        result.calibrationError =
            update.error;
      }
    }
#endif
  }

  result.stateRemoved =
      ClientStateStore::remove(
          executionSlot);

  if (!result.stateRemoved) {
    result.completionError =
        "Unable to remove local execution state";
  }

  return result;
}

} // namespace openpuzzle::client
