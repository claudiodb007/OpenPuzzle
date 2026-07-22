#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle::client;

namespace {

ClientExecutionState makeState(
    const std::filesystem::path& workspace,
    int pid) {
  ClientExecutionState state;

  state.active = true;

  state.assignmentId =
      "11111111-1111-4111-8111-111111111111";

  state.clientId =
      "22222222-2222-4222-8222-222222222222";

  state.puzzle = 71;
  state.rangeId = 999999;
  state.pid = pid;

  state.target =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

  state.start =
      "400000000000000000";

  state.end =
      "40000000FFFFFFFFFF";

  state.engine =
      "BitCrack";

  state.backend =
      "CUDA";

  state.workspace =
      workspace.string();

  state.command =
      "test";

  return state;
}

void writeFile(
    const std::filesystem::path& path,
    const std::string& content) {
  std::ofstream output(path);
  output << content;
}

} // namespace

int main() {
  const char* originalHome =
      std::getenv("HOME");

  const bool hadHome =
      originalHome != nullptr;

  const std::string savedHome =
      hadHome
          ? originalHome
          : "";

  const auto temporaryHome =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-execution-sync-" +
          std::to_string(getpid())
      );

  const auto workspace =
      temporaryHome /
      "workspace";

  std::filesystem::remove_all(
      temporaryHome);

  std::filesystem::create_directories(
      workspace);

  assert(
      setenv(
          "HOME",
          temporaryHome.string().c_str(),
          1) == 0);

  ExecutionSyncService service;

  assert(
      ExecutionSyncService::classifyProgressError(
          "assignment_not_found") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyProgressError(
          "assignment_lease_expired") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyProgressError(
          "invalid_keys_checked") ==
      AssignmentUploadStatus::PermanentFailure);

  assert(
      ExecutionSyncService::classifyProgressError(
          "progress_failed") ==
      AssignmentUploadStatus::TemporaryFailure);

  assert(
      ExecutionSyncService::classifyProgressError(
          "") ==
      AssignmentUploadStatus::TemporaryFailure);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "assignment_not_found") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "invalid_assignment_state") ==
      AssignmentUploadStatus::AssignmentRejected);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "invalid_exit_code") ==
      AssignmentUploadStatus::PermanentFailure);

  assert(
      ExecutionSyncService::classifyCompletionError(
          "completion_failed") ==
      AssignmentUploadStatus::TemporaryFailure);

  /*
   * Sem estado local não existe sincronização.
   */
  {
    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(!result.hasState);
  }

  /*
   * found.txt não vazio suspende toda a
   * sincronização e preserva o estado.
   */
  {
    const auto state =
        makeState(
            workspace,
            999999999);

    assert(
        ClientStateStore::save(
            state));

    writeFile(
        workspace / "found.txt",
        "synthetic-test-secret\n");

    writeFile(
        workspace / "exit.code",
        "0\n");

    const auto solution =
        ExecutionSyncService::solutionFile(
            workspace.string());

    assert(solution);

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(result.solutionFound);
    assert(
        result.solutionPath ==
        (workspace / "found.txt").string());

    assert(!result.completionUploaded);
    assert(!result.stateRemoved);
    assert(ClientStateStore::load());

    std::filesystem::remove(
        workspace / "found.txt");

    std::filesystem::remove(
        workspace / "exit.code");

    assert(
        ClientStateStore::remove());
  }

  /*
   * Ficheiro vazio não representa solução.
   */
  {
    writeFile(
        workspace / "found.txt",
        "");

    assert(
        !ExecutionSyncService::solutionFile(
            workspace.string()));

    std::filesystem::remove(
        workspace / "found.txt");
  }

  /*
   * Processo terminado com exit code diferente de zero:
   * tenta reportar failed e preserva o estado quando
   * o servidor não está disponível.
   */
  {
    const auto state =
        makeState(
            workspace,
            999999999);

    assert(
        ClientStateStore::save(
            state));

    writeFile(
        workspace / "exit.code",
        "7\n");

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(!result.running);
    assert(result.hasExitCode);
    assert(result.exitCode == 7);

    assert(!result.completionUploaded);
    assert(!result.completionError.empty());
    assert(!result.stateRemoved);

    assert(
        ClientStateStore::load());

    assert(
        ClientStateStore::remove());
  }

  /*
   * Processo terminado sem exit.code:
   * deve manter o estado e aguardar o ficheiro.
   */
  {
    std::filesystem::remove(
        workspace / "exit.code");

    const auto state =
        makeState(
            workspace,
            999999999);

    assert(
        ClientStateStore::save(
            state));

    const auto result =
        service.tick(
            "http://127.0.0.1:1");

    assert(result.hasState);
    assert(!result.running);
    assert(!result.hasExitCode);

    assert(!result.completionUploaded);
    assert(!result.stateRemoved);

    assert(
        ClientStateStore::load());

    assert(
        ClientStateStore::remove());
  }


  /*
   * BitCrack atualiza o progresso com carriage
   * return. Deve ser usada a última amostra e não
   * a primeira ocorrência da linha acumulada.
   */
  {
    writeFile(
        workspace / "bitcrack.log",
        "AMD Radeon RX 57 | 1 target "
        "400.00 MKey/s "
        "(775,946,240 total) [00:00:01]\r"
        "AMD Radeon RX 57 | 1 target "
        "425.64 MKey/s "
        "(15,518,924,800 total) [00:00:34]\r"
        "AMD Radeon RX 57 | 1 target "
        "428.23 MKey/s "
        "(123,354,480,640 total) [00:04:48]\r");

    const auto progress =
        ExecutionSyncService::latestProgress(
            workspace.string());

    assert(progress);

    assert(
        progress->speedMKeys > 428.22 &&
        progress->speedMKeys < 428.24);

    assert(
        progress->keysChecked ==
        "123354480640");

    std::filesystem::remove(
        workspace / "bitcrack.log");
  }

  /*
   * Conclusão exige simultaneamente uma amostra real
   * de progresso e o marcador final do BitCrack.
   */
  {
    writeFile(
        workspace / "bitcrack.log",
        "[Info] Reached end of keyspace\n");

    assert(
        !ExecutionSyncService::hasCompletionProof(
            workspace.string()));

    writeFile(
        workspace / "bitcrack.log",
        "GPU | 1 target 425.64 MKey/s "
        "(123,354,480,640 total) [00:04:48]\r");

    assert(
        !ExecutionSyncService::hasCompletionProof(
            workspace.string()));

    writeFile(
        workspace / "bitcrack.log",
        "GPU | 1 target 425.64 MKey/s "
        "(123,354,480,640 total) [00:04:48]\r"
        "[Info] Reached end of keyspace\n");

    assert(
        ExecutionSyncService::hasCompletionProof(
            workspace.string()));

    const auto count =
        ExecutionSyncService::assignedKeyCount(
            "400000000000000000",
            "40000000FFFFFFFFFF");

    assert(count);
    assert(*count == "1099511627776");

    assert(
        !ExecutionSyncService::assignedKeyCount(
            "20",
            "10"));

    assert(
        !ExecutionSyncService::assignedKeyCount(
            "not-hex",
            "20"));

    std::filesystem::remove(
        workspace / "bitcrack.log");
  }

  /*
   * KeyHunt usa um log próprio. A métrica pública
   * conta dois hashes comprimidos por escalar; o
   * parser devolve chaves privadas reais e exige End.
   */
  {
    writeFile(
        workspace / "keyhunt.log",
        "[+] Total 12509184 keys in 2 seconds: "
        "~6 Mkeys/s (6254592 keys/s)\r"
        "End\n");

    const auto progress =
        ExecutionSyncService::latestProgress(
            workspace.string(),
            "KeyHunt");

    assert(progress);
    assert(
        progress->keysChecked ==
        "6254592");
    assert(
        progress->speedMKeys > 3.127295 &&
        progress->speedMKeys < 3.127297);

    assert(
        ExecutionSyncService::hasCompletionProof(
            workspace.string(),
            "KeyHunt"));

    assert(
        !ExecutionSyncService::hasCompletionProof(
            workspace.string(),
            "BitCrack"));

    writeFile(
        workspace / "keyhunt.log",
        "[+] Total 12509184 keys in 2 seconds: "
        "~6 Mkeys/s (6254592 keys/s)\r");

    assert(
        !ExecutionSyncService::hasCompletionProof(
            workspace.string(),
            "KeyHunt"));

    std::filesystem::remove(
        workspace / "keyhunt.log");
  }

  std::filesystem::remove_all(
      temporaryHome);

  if (hadHome) {
    assert(
        setenv(
            "HOME",
            savedHome.c_str(),
            1) == 0);
  } else {
    assert(
        unsetenv("HOME") == 0);
  }

  std::cout
      << "ExecutionSyncServiceTests passed\n";

  return 0;
}
