#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/KangarooCheckpointRecoveryFlow.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

namespace {

void writeFile(
    const std::filesystem::path &path,
    const std::string &value) {
  std::ofstream output(path, std::ios::trunc);
  assert(output);
  output << value;
  output.close();
  assert(output);
}

client::ClientExecutionState stateFor(
    const std::filesystem::path &workspace) {
  client::ClientExecutionState state;
  state.active = true;
  state.assignmentId = "11111111-1111-4111-8111-111111111111";
  state.clientId = "22222222-2222-4222-8222-222222222222";
  state.puzzle = 140;
  state.rangeId = 9001;
  state.pid = 999999;
  state.device = 0;
  state.target = "synthetic-address";
  state.publicKey =
      "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640";
  state.kangarooWalkSeed = "0123456789ABCDEF";
  state.kangarooGeneration = 9;
  state.start = "100000000";
  state.end = "1FFFFFFFF";
  state.engine = "Kangaroo";
  state.backend = "CUDA";
  state.workspace = workspace.string();
  state.command = "touch command-must-never-run";
  return state;
}

} // namespace

int main() {
  const char *oldHomeValue = std::getenv("HOME");
  const char *oldPathValue = std::getenv("PATH");
  const std::string oldHome = oldHomeValue ? oldHomeValue : "";
  const std::string oldPath = oldPathValue ? oldPathValue : "";
  const auto home =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-kangaroo-flow-" + std::to_string(getpid()));
  const auto assignment = "11111111-1111-4111-8111-111111111111";
  const auto workspace =
      home / ".local/share/OpenPuzzle/assignments" / assignment;
  const auto executable = home / "psckangaroo";
  const auto bin = home / "bin";
  const auto checkpoint = workspace / "kangaroo.checkpoint";

  std::filesystem::remove_all(home);
  std::filesystem::create_directories(workspace);
  std::filesystem::create_directories(bin);
  std::filesystem::permissions(
      workspace,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace);
  assert(setenv("HOME", home.c_str(), 1) == 0);

  writeFile(executable, "#!/bin/sh\nsleep 30\n");
  std::filesystem::permissions(
      executable,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace);
  writeFile(checkpoint, "checkpoint-data\n");
  std::filesystem::permissions(
      checkpoint,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace);
  writeFile(
      workspace / "kangaroo.log",
      "HUNT: Speed: 2.75 GKeys/s | DPs: 42M | Time: 0d 01h 05m\n");

  const auto curl = bin / "curl";
  writeFile(
      curl,
      "#!/bin/sh\n"
      "case \"$OPENPUZZLE_FAKE_CURL_MODE\" in\n"
      "  accepted) exit 0 ;;\n"
      "  rejected) printf '%s\\n' '{\"error\":\"assignment_lease_expired\",\"message\":\"expired\"}'; exit 22 ;;\n"
      "  *) printf '%s\\n' 'temporary network failure'; exit 28 ;;\n"
      "esac\n");
  std::filesystem::permissions(
      curl,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace);
  const std::string testPath = bin.string() + ":" + oldPath;
  assert(setenv("PATH", testPath.c_str(), 1) == 0);

  const auto state = stateFor(workspace);

  assert(setenv("OPENPUZZLE_FAKE_CURL_MODE", "temporary", 1) == 0);
  const auto retry = KangarooCheckpointRecoveryFlow::recover(
      "https://server.test", state, executable, workspace);
  assert(retry.status == KangarooCheckpointRecoveryFlowStatus::Retry);
  assert(!client::ClientStateStore::load());

  assert(setenv("OPENPUZZLE_FAKE_CURL_MODE", "rejected", 1) == 0);
  const auto rejected = KangarooCheckpointRecoveryFlow::recover(
      "https://server.test", state, executable, workspace);
  assert(rejected.status ==
         KangarooCheckpointRecoveryFlowStatus::Rejected);
  assert(!client::ClientStateStore::load());

  auto unsafe = state;
  unsafe.workspace = (home / "unexpected").string();
  const auto invalid = KangarooCheckpointRecoveryFlow::recover(
      "https://server.test", unsafe, executable, workspace);
  assert(invalid.status == KangarooCheckpointRecoveryFlowStatus::Invalid);
  assert(!client::ClientStateStore::load());

  assert(setenv("OPENPUZZLE_FAKE_CURL_MODE", "accepted", 1) == 0);
  const auto resumed = KangarooCheckpointRecoveryFlow::recover(
      "https://server.test", state, executable, workspace);
  assert(resumed.resumed());
  assert(resumed.state.pid > 0);
  assert(resumed.state.kangarooWalkSeed != state.kangarooWalkSeed);
  assert(resumed.state.kangarooGeneration == 10);
  assert(resumed.state.command.find("-seed '" +
                                    resumed.state.kangarooWalkSeed + "'") !=
         std::string::npos);
  assert(resumed.state.command.find(state.command) == std::string::npos);
  assert(client::ClientStateStore::load());
  assert(std::filesystem::is_regular_file(checkpoint));

  assert(ExecutionStopper().stop(workspace.string()));
  assert(client::ClientStateStore::remove());

  if (oldHomeValue) {
    assert(setenv("HOME", oldHome.c_str(), 1) == 0);
  } else {
    unsetenv("HOME");
  }
  if (oldPathValue) {
    assert(setenv("PATH", oldPath.c_str(), 1) == 0);
  } else {
    unsetenv("PATH");
  }
  unsetenv("OPENPUZZLE_FAKE_CURL_MODE");
  std::filesystem::remove_all(home);

  std::cout << "KangarooCheckpointRecoveryFlowTests passed\n";
  return 0;
}
