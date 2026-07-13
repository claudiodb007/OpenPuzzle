#include "openpuzzle/client/ClientStateStore.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle::client;

namespace {

ClientExecutionState makeValidState() {
  ClientExecutionState state;

  state.active = true;

  state.assignmentId =
      "11111111-1111-4111-8111-111111111111";

  state.clientId =
      "22222222-2222-4222-8222-222222222222";

  state.puzzle = 71;
  state.rangeId = 84521;
  state.pid = 12345;

  state.target =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

  state.start =
      "400000070000000000";

  state.end =
      "40000007FFFFFFFFFF";

  state.engine =
      "BitCrack";

  state.backend =
      "CUDA";

  state.workspace =
      "/tmp/openpuzzle-client-state-workspace";

  state.command =
      "echo \"OpenPuzzle\" \\\n"
      "--keyspace 400000070000000000:"
      "40000007FFFFFFFFFF";

  return state;
}

void assertEqual(
    const ClientExecutionState& expected,
    const ClientExecutionState& actual) {
  assert(actual.active == expected.active);

  assert(
      actual.assignmentId ==
      expected.assignmentId);

  assert(
      actual.clientId ==
      expected.clientId);

  assert(actual.puzzle == expected.puzzle);
  assert(actual.rangeId == expected.rangeId);
  assert(actual.pid == expected.pid);

  assert(actual.target == expected.target);
  assert(actual.start == expected.start);
  assert(actual.end == expected.end);

  assert(actual.engine == expected.engine);
  assert(actual.backend == expected.backend);

  assert(
      actual.workspace ==
      expected.workspace);

  assert(
      actual.command ==
      expected.command);
}

} // namespace

int main() {
  const char* originalHome =
      std::getenv("HOME");

  const std::string savedHome =
      originalHome
          ? originalHome
          : "";

  const bool hadHome =
      originalHome != nullptr;

  const auto temporaryHome =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-client-state-" +
          std::to_string(getpid())
      );

  std::filesystem::remove_all(
      temporaryHome);

  std::filesystem::create_directories(
      temporaryHome);

  assert(
      setenv(
          "HOME",
          temporaryHome.string().c_str(),
          1) == 0);

  assert(!ClientStateStore::load());

  auto state =
      makeValidState();

  assert(state.valid());

  assert(
      ClientStateStore::save(
          state));

  assert(
      std::filesystem::is_regular_file(
          ClientStateStore::path()));

  const auto loaded =
      ClientStateStore::load();

  assert(loaded);

  assertEqual(
      state,
      *loaded);

  ClientExecutionState invalid;

  assert(!invalid.valid());

  assert(
      !ClientStateStore::save(
          invalid));

  /*
   * O estado válido anterior não pode ser
   * destruído por uma tentativa inválida.
   */
  const auto loadedAfterInvalidSave =
      ClientStateStore::load();

  assert(loadedAfterInvalidSave);

  assertEqual(
      state,
      *loadedAfterInvalidSave);

  assert(
      ClientStateStore::remove());

  assert(
      !std::filesystem::exists(
          ClientStateStore::path()));

  /*
   * A remoção é idempotente.
   */
  assert(
      ClientStateStore::remove());

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
      << "ClientStateStoreTests passed\n";

  return 0;
}
