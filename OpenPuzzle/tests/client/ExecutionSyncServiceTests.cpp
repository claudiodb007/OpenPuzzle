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
   * Processo terminado com exit code diferente de zero:
   * não deve contactar o servidor nem remover o estado.
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
