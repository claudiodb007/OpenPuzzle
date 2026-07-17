#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

namespace {

void assertPrivateDirectory(
    const std::filesystem::path &path) {
  const auto permissions =
      std::filesystem::status(
          path).permissions();

  assert(
      (permissions &
       std::filesystem::perms::owner_all) ==
      std::filesystem::perms::owner_all);

  assert(
      (permissions &
       std::filesystem::perms::group_all) ==
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::others_all) ==
      std::filesystem::perms::none);
}

void assertPrivateFile(
    const std::filesystem::path &path) {
  const auto permissions =
      std::filesystem::status(
          path).permissions();

  assert(
      (permissions &
       std::filesystem::perms::owner_read) !=
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::owner_write) !=
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::group_all) ==
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::others_all) ==
      std::filesystem::perms::none);
}

} // namespace

int main() {
  const char *originalHome =
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
          "openpuzzle-local-security-" +
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

  const auto configDirectory =
      temporaryHome /
      ".config" /
      "OpenPuzzle";

  const auto dataDirectory =
      temporaryHome /
      ".local" /
      "share" /
      "OpenPuzzle";

  const auto identity =
      client::ClientIdentity::
          loadOrCreate();

  assert(!identity.empty());
  assertPrivateDirectory(
      configDirectory);
  assertPrivateFile(
      configDirectory /
      "client.id");

  Configuration configuration;

  assert(
      ConfigurationManager::save(
          configuration));

  assertPrivateFile(
      configDirectory /
      "config.json");

  client::ClientExecutionState state;

  state.active = true;
  state.assignmentId =
      "11111111-1111-4111-8111-111111111111";
  state.clientId = identity;
  state.puzzle = 71;
  state.rangeId = 999999;
  state.pid = static_cast<int>(
      getpid());
  state.target =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
  state.start =
      "400000000000000000";
  state.end =
      "40000000FFFFFFFFFF";
  state.engine = "BitCrack";
  state.backend = "CUDA";
  state.workspace =
      (dataDirectory /
       "assignments" /
       state.assignmentId).string();
  state.command =
      "synthetic-command";

  assert(
      client::ClientStateStore::save(
          state));

  assertPrivateDirectory(
      dataDirectory);
  assertPrivateFile(
      dataDirectory /
      "client.state");

  assert(
      ClientRuntimeControl::acquire());

  assertPrivateFile(
      dataDirectory /
      "runtime.pid");

  assert(
      ClientRuntimeControl::release());

  Database database;

  const auto databasePath =
      dataDirectory /
      "openpuzzle.db";

  assert(
      database.open(
          databasePath.string()));

  assertPrivateFile(
      databasePath);

  database.close();

  assert(
      client::ClientStateStore::remove());

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
      << "LocalStorageSecurityTests passed\n";

  return 0;
}
