#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/KangarooCheckpointRecoveryCoordinator.hpp"
#include "openpuzzle/runtime/KangarooCheckpointRecoveryPlanner.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

namespace {

void writeFile(const std::filesystem::path &path, const std::string &value) {
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
  const std::string oldHome = oldHomeValue ? oldHomeValue : "";
  const auto home =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-kangaroo-coordinator-" + std::to_string(getpid()));
  const auto assignment = "11111111-1111-4111-8111-111111111111";
  const auto workspace =
      home / ".local/share/OpenPuzzle/assignments" / assignment;
  const auto executable = home / "psckangaroo";
  const auto checkpoint = workspace / "kangaroo.checkpoint";

  std::filesystem::remove_all(home);
  std::filesystem::create_directories(workspace);
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

  const auto state = stateFor(workspace);
  const auto plan = KangarooCheckpointRecoveryPlanner::build(
      state, executable, workspace);
  assert(plan.eligible);

  KangarooRecoveryLeaseResult retryLease;
  retryLease.status = KangarooRecoveryLeaseStatus::Retry;
  const auto blocked = KangarooCheckpointRecoveryCoordinator::resume(
      state, plan, retryLease);
  assert(!blocked.resumed);
  assert(!client::ClientStateStore::load());

  KangarooRecoveryLeaseResult acceptedLease;
  acceptedLease.status = KangarooRecoveryLeaseStatus::Accepted;
  acceptedLease.speedMKeys = 2750.0;

  const auto resumed = KangarooCheckpointRecoveryCoordinator::resume(
      state, plan, acceptedLease);
  assert(resumed.resumed);
  assert(resumed.state.pid > 0);
  assert(resumed.state.pid != state.pid);
  assert(!resumed.state.bootId.empty());
  assert(resumed.state.processStartTime > 0);
  assert(resumed.state.command == plan.request.command);
  assert(resumed.state.kangarooWalkSeed == plan.walkSeed);
  assert(resumed.state.kangarooWalkSeed != state.kangarooWalkSeed);
  assert(resumed.state.kangarooGeneration == 10);
  assert(resumed.state.command.find(state.command) == std::string::npos);

  const auto persisted = client::ClientStateStore::load();
  assert(persisted);
  assert(persisted->pid == resumed.state.pid);
  assert(persisted->bootId == resumed.state.bootId);
  assert(persisted->processStartTime == resumed.state.processStartTime);
  assert(persisted->kangarooWalkSeed == resumed.state.kangarooWalkSeed);
  assert(persisted->kangarooGeneration == 10);
  assert(std::filesystem::is_regular_file(checkpoint));
  assert(std::filesystem::file_size(checkpoint) > 0);

  assert(ExecutionStopper().stop(workspace.string()));
  assert(client::ClientStateStore::remove());

  if (oldHomeValue) {
    assert(setenv("HOME", oldHome.c_str(), 1) == 0);
  } else {
    unsetenv("HOME");
  }

  std::filesystem::remove_all(home);
  std::cout << "KangarooCheckpointRecoveryCoordinatorTests passed\n";
  return 0;
}
