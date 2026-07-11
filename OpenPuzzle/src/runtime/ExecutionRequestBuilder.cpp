#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"

#include "openpuzzle/core/WorkspaceManager.hpp"
#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/engines/EngineManager.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace openpuzzle {

static std::string normalizeEngineId(
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

StartExecutionRequest ExecutionRequestBuilder::build(
    const PuzzleRecord& puzzle,
    const RangeRecord& range,
    const JobRecord& job,
    const WorkerEngineCapability& capability,
    const std::string& executable,
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

  const std::string engineId =
      normalizeEngineId(capability.engine);

  WorkspaceManager workspaceManager(
      std::filesystem::path(workspace)
          .parent_path()
          .parent_path());

  const auto outputFile =
      workspaceManager
          .foundFile(job.id)
          .string();

  const auto logFile =
      workspaceManager
          .engineLog(job.id, engineId)
          .string();

  EngineLaunchRequest launchRequest;
  launchRequest.puzzle = puzzle;
  launchRequest.range = range;
  launchRequest.device = capability.device;
  launchRequest.blocks = capability.blocks;
  launchRequest.threads = capability.threads;
  launchRequest.points = capability.points;
  launchRequest.workspace = workspace;
  launchRequest.outputFile = outputFile;
  launchRequest.logFile = logFile;

  EngineManager engineManager;

  auto engine = engineManager.create(
      engineId,
      executable);

  if (!engine) {
    throw std::runtime_error(
        "Could not create search engine: " +
        capability.engine);
  }

  StartExecutionRequest request;
  request.puzzleId = puzzle.id;
  request.jobId = job.id;
  request.rangeId = range.id;

  request.engine = capability.engine;
  request.backend = capability.backend;

  request.device = capability.device;
  request.blocks = capability.blocks;
  request.threads = capability.threads;
  request.points = capability.points;

  request.workspace = workspace;
  request.command =
      engine->buildCommand(launchRequest);
  request.echoOutput = true;

  return request;
}

} // namespace openpuzzle
