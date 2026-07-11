#include "openpuzzle/engines/EngineLaunchBuilder.hpp"

#include "openpuzzle/core/WorkspaceManager.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace openpuzzle {

static std::string normalizeEngineLogName(
    std::string engine) {
  std::transform(
      engine.begin(),
      engine.end(),
      engine.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  return engine;
}

EngineLaunchRequest EngineLaunchBuilder::build(
    const PuzzleRecord& puzzle,
    const RangeRecord& range,
    const JobRecord& job,
    const WorkerEngineCapability& capability,
    const std::string& workspace) const {
  if (!capability.available) {
    throw std::runtime_error(
        "Worker engine capability is unavailable");
  }

  if (capability.engine.empty()) {
    throw std::runtime_error(
        "Worker engine capability has no engine");
  }

  if (capability.backend.empty()) {
    throw std::runtime_error(
        "Worker engine capability has no backend");
  }

  if (!capability.hasLaunchProfile()) {
    throw std::runtime_error(
        "Worker engine capability has no launch profile");
  }

  WorkspaceManager workspaceManager(
      std::filesystem::path(workspace)
          .parent_path()
          .parent_path());

  const auto engineLogName =
      normalizeEngineLogName(
          capability.engine);

  EngineLaunchRequest request;

  request.puzzle = puzzle;
  request.range = range;

  request.device = capability.device;
  request.blocks = capability.blocks;
  request.threads = capability.threads;
  request.points = capability.points;

  request.workspace = workspace;

  request.targetFile =
      (std::filesystem::path(workspace) /
       "targets.txt")
          .string();

  {
    std::filesystem::create_directories(
        workspace);

    std::ofstream targets(
        request.targetFile,
        std::ios::trunc);

    if (!targets.is_open()) {
      throw std::runtime_error(
          "Could not create engine target file");
    }

    targets << puzzle.address << "\n";
  }

  request.outputFile =
      workspaceManager
          .foundFile(job.id)
          .string();

  request.logFile =
      workspaceManager
          .engineLog(
              job.id,
              engineLogName)
          .string();

  return request;
}

} // namespace openpuzzle
