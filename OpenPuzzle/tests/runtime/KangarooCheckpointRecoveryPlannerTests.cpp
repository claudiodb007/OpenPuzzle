#include "openpuzzle/runtime/KangarooCheckpointRecoveryPlanner.hpp"
#include "openpuzzle/runtime/KangarooWalkSeed.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

namespace {

client::ClientExecutionState validState(
    const std::filesystem::path &workspace) {
  client::ClientExecutionState state;
  state.active = true;
  state.assignmentId = "11111111-1111-4111-8111-111111111111";
  state.clientId = "22222222-2222-4222-8222-222222222222";
  state.puzzle = 140;
  state.rangeId = 9001;
  state.pid = 12345;
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
  state.command = "persisted command must never run";
  return state;
}

void writeFile(const std::filesystem::path &path, const std::string &value) {
  std::ofstream output(path, std::ios::trunc);
  assert(output);
  output << value;
  output.close();
  assert(output);
}

void privateFile(const std::filesystem::path &path) {
  std::filesystem::permissions(
      path,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace);
}

} // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-kangaroo-recovery-plan-" + std::to_string(getpid()));
  const auto assignment = "11111111-1111-4111-8111-111111111111";
  const auto workspace = root / "assignments" / assignment;
  const auto executable = root / "psckangaroo";
  const auto checkpoint = workspace / "kangaroo.checkpoint";

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(workspace);
  std::filesystem::permissions(
      workspace,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace);

  writeFile(executable, "#!/bin/sh\nexit 0\n");
  std::filesystem::permissions(
      executable,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace);

  writeFile(checkpoint, "checkpoint-data\n");
  privateFile(checkpoint);

  const auto state = validState(workspace);
  const auto forbidden = root / "command-must-never-run";
  auto maliciousState = state;
  maliciousState.command = "touch " + forbidden.string();
  const auto plan = KangarooCheckpointRecoveryPlanner::build(
      maliciousState, executable, workspace);

  assert(plan.eligible);
  assert(plan.error.empty());
  assert(plan.request.executionId == state.rangeId);
  assert(plan.request.puzzleId == state.puzzle);
  assert(plan.request.workspace == workspace.string());
  assert(KangarooWalkSeed::valid(plan.walkSeed));
  assert(plan.walkSeed != state.kangarooWalkSeed);
  assert(plan.generation == 10);
  assert(plan.request.walkSeed == plan.walkSeed);
  assert(plan.request.command.find("-seed '" + plan.walkSeed + "'") !=
         std::string::npos);
  assert(plan.request.command.find(state.publicKey) != std::string::npos);
  assert(plan.request.command.find("-loadwild") != std::string::npos);
  assert(plan.request.command.find(maliciousState.command) ==
         std::string::npos);
  assert(!std::filesystem::exists(forbidden));

  auto invalidSeed = state;
  invalidSeed.kangarooWalkSeed = "invalid";
  assert(!KangarooCheckpointRecoveryPlanner::build(
      invalidSeed, executable, workspace).eligible);

  auto overflow = state;
  overflow.kangarooGeneration =
      std::numeric_limits<std::uint64_t>::max();
  assert(!KangarooCheckpointRecoveryPlanner::build(
      overflow, executable, workspace).eligible);

  auto invalidPublicKey = state;
  invalidPublicKey.publicKey = "invalid";
  assert(!KangarooCheckpointRecoveryPlanner::build(
      invalidPublicKey, executable, workspace).eligible);

  auto wrongWorkspace = state;
  wrongWorkspace.workspace = (root / "wrong" / assignment).string();
  assert(!KangarooCheckpointRecoveryPlanner::build(
      wrongWorkspace, executable, workspace).eligible);

  std::filesystem::permissions(
      checkpoint,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::group_read,
      std::filesystem::perm_options::replace);
  assert(!KangarooCheckpointRecoveryPlanner::build(
      state, executable, workspace).eligible);
  privateFile(checkpoint);

  const auto outside = root / "outside.checkpoint";
  writeFile(outside, "outside\n");
  std::filesystem::remove(checkpoint);
  std::filesystem::create_symlink(outside, checkpoint);
  assert(!KangarooCheckpointRecoveryPlanner::build(
      state, executable, workspace).eligible);
  std::filesystem::remove(checkpoint);
  writeFile(checkpoint, "checkpoint-data\n");
  privateFile(checkpoint);

  writeFile(workspace / "exit.code", "0\n");
  assert(!KangarooCheckpointRecoveryPlanner::build(
      state, executable, workspace).eligible);
  std::filesystem::remove(workspace / "exit.code");

  writeFile(workspace / "RESULTS.TXT", "private-result-material\n");
  assert(!KangarooCheckpointRecoveryPlanner::build(
      state, executable, workspace).eligible);
  std::filesystem::remove(workspace / "RESULTS.TXT");

  auto linear = state;
  linear.engine = "BitCrack";
  assert(!KangarooCheckpointRecoveryPlanner::build(
      linear, executable, workspace).eligible);

  const auto executableLink = root / "psckangaroo-link";
  std::filesystem::create_symlink(executable, executableLink);
  assert(!KangarooCheckpointRecoveryPlanner::build(
      state, executableLink, workspace).eligible);

  assert(std::filesystem::is_regular_file(checkpoint));
  assert(std::filesystem::file_size(checkpoint) > 0);

  std::filesystem::remove_all(root);
  std::cout << "KangarooCheckpointRecoveryPlannerTests passed\n";
  return 0;
}
