#include "openpuzzle/runtime/KangarooCheckpointRecoveryPlanner.hpp"

#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/engines/kangaroo/KangarooEngine.hpp"
#include "openpuzzle/runtime/KangarooWalkSeed.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>

namespace openpuzzle {
namespace {

std::string lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });
  return value;
}

bool validUuid(const std::string &value) {
  if (value.size() != 36 || value[8] != '-' || value[13] != '-' ||
      value[18] != '-' || value[23] != '-') {
    return false;
  }

  for (std::size_t index = 0; index < value.size(); ++index) {
    if (index == 8 || index == 13 || index == 18 || index == 23) {
      continue;
    }

    if (std::isxdigit(static_cast<unsigned char>(value[index])) == 0) {
      return false;
    }
  }

  return true;
}

bool hasPublicPermissions(std::filesystem::perms permissions) {
  using perms = std::filesystem::perms;
  return (permissions & (perms::group_all | perms::others_all)) != perms::none;
}

bool nonEmptyRegularFile(const std::filesystem::path &path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error || !std::filesystem::is_regular_file(status)) {
    return false;
  }

  const auto size = std::filesystem::file_size(path, error);
  return !error && size > 0;
}

KangarooCheckpointRecoveryPlan rejected(std::string error) {
  KangarooCheckpointRecoveryPlan result;
  result.error = std::move(error);
  return result;
}

} // namespace

KangarooCheckpointRecoveryPlan
KangarooCheckpointRecoveryPlanner::build(
    const client::ClientExecutionState &state,
    const std::filesystem::path &executable,
    const std::filesystem::path &expectedWorkspace) {
  if (!state.valid()) {
    return rejected("Invalid local execution state");
  }

  if (!validUuid(state.assignmentId)) {
    return rejected("Invalid assignment identifier");
  }

  const auto engine = lower(state.engine);
  const auto backend = lower(state.backend);
  if ((engine != "kangaroo" && engine != "psckangaroo") ||
      backend != "cuda") {
    return rejected("Execution is not a CUDA Kangaroo assignment");
  }

  const auto workspace = std::filesystem::path(state.workspace).lexically_normal();
  const auto expected = expectedWorkspace.lexically_normal();
  if (expected.empty() || workspace != expected ||
      expected.filename() != state.assignmentId) {
    return rejected("Assignment workspace does not match local state");
  }

  std::error_code error;
  const auto workspaceStatus = std::filesystem::symlink_status(expected, error);
  if (error || std::filesystem::is_symlink(workspaceStatus) ||
      !std::filesystem::is_directory(workspaceStatus) ||
      hasPublicPermissions(workspaceStatus.permissions())) {
    return rejected("Assignment workspace is not private and direct");
  }

  const auto executableStatus =
      std::filesystem::symlink_status(executable, error);
  if (error || std::filesystem::is_symlink(executableStatus) ||
      !std::filesystem::is_regular_file(executableStatus) ||
      access(executable.c_str(), X_OK) != 0) {
    return rejected("PSCKangaroo executable is not direct and executable");
  }

  const auto checkpoint = expected / "kangaroo.checkpoint";
  const auto checkpointStatus =
      std::filesystem::symlink_status(checkpoint, error);
  if (error || std::filesystem::is_symlink(checkpointStatus) ||
      !std::filesystem::is_regular_file(checkpointStatus) ||
      hasPublicPermissions(checkpointStatus.permissions())) {
    return rejected("Kangaroo checkpoint is not a private regular file");
  }

  const auto checkpointSize = std::filesystem::file_size(checkpoint, error);
  if (error || checkpointSize == 0) {
    return rejected("Kangaroo checkpoint is empty");
  }

  for (const auto &name : {"exit.code", "found.txt", "RESULTS.TXT"}) {
    const auto path = expected / name;
    const auto status = std::filesystem::symlink_status(path, error);

    if (!error && std::filesystem::is_symlink(status)) {
      return rejected("Unsafe recovery workspace entry: " +
                      std::string(name));
    }

    error.clear();
  }

  if (std::filesystem::exists(expected / "exit.code", error) && !error) {
    return rejected("Execution already has an exit marker");
  }
  error.clear();

  if (nonEmptyRegularFile(expected / "found.txt") ||
      nonEmptyRegularFile(expected / "RESULTS.TXT")) {
    return rejected("Recovery workspace contains result material");
  }

  if (!state.kangarooWalkSeed.empty() &&
      !KangarooWalkSeed::valid(state.kangarooWalkSeed)) {
    return rejected("Invalid previous Kangaroo walk seed");
  }

  if (state.kangarooGeneration ==
      std::numeric_limits<std::uint64_t>::max()) {
    return rejected("Kangaroo recovery generation overflow");
  }

  std::string walkSeed;
  try {
    do {
      walkSeed = KangarooWalkSeed::generate();
    } while (walkSeed == state.kangarooWalkSeed);
  } catch (const std::exception &exception) {
    return rejected(std::string("Unable to generate recovery walk seed: ") +
                    exception.what());
  }

  const auto generation = state.kangarooGeneration + 1;

  EngineLaunchRequest launch;
  launch.engine = "Kangaroo";
  launch.backend = "CUDA";
  launch.publicKey = state.publicKey;
  launch.walkSeed = walkSeed;
  launch.startKey = state.start;
  launch.endKey = state.end;
  launch.device = state.device;
  launch.workspace = expected.string();
  launch.outputFile = (expected / "found.txt").string();
  launch.logFile = (expected / "kangaroo.log").string();

  std::string command;
  try {
    command = KangarooEngine(executable.string()).buildCommand(launch);
  } catch (const std::exception &exception) {
    return rejected(std::string("Invalid Kangaroo recovery metadata: ") +
                    exception.what());
  }

  KangarooCheckpointRecoveryPlan result;
  result.eligible = true;
  result.walkSeed = walkSeed;
  result.generation = generation;
  result.request.executionId = state.rangeId;
  result.request.puzzleId = state.puzzle;
  result.request.jobId = state.rangeId;
  result.request.rangeId = state.rangeId;
  result.request.engine = "Kangaroo";
  result.request.backend = "CUDA";
  result.request.walkSeed = walkSeed;
  result.request.device = state.device;
  result.request.workspace = expected.string();
  result.request.command = std::move(command);
  result.request.echoOutput = true;
  return result;
}

} // namespace openpuzzle
