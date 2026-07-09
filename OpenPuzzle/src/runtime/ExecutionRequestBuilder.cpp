#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"

#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <stdexcept>

namespace openpuzzle {

StartExecutionRequest ExecutionRequestBuilder::build(
    const PuzzleRecord& puzzle,
    const RangeRecord& range,
    const JobRecord& job,
    const std::string& executable,
    const std::string& workspace) const {

  WorkspaceManager workspaceManager(
      std::filesystem::path(workspace).parent_path().parent_path());

  auto outputFile = workspaceManager.foundFile(job.id).string();
  auto logFile = workspaceManager.engineLog(job.id, "bitcrack").string();

  EngineLaunchRequest launchRequest;
  launchRequest.puzzle = puzzle;
  launchRequest.range = range;
  launchRequest.device = 0;
  launchRequest.blocks = 256;
  launchRequest.threads = 256;
  launchRequest.points = 1024;
  launchRequest.workspace = workspace;
  launchRequest.outputFile = outputFile;
  launchRequest.logFile = logFile;

  EngineManager engineManager;
  auto engine = engineManager.create("bitcrack", executable);

  if (!engine) {
    throw std::runtime_error("Could not create BitCrack engine");
  }

  StartExecutionRequest request;
  request.executionId = 0;
  request.puzzleId = puzzle.id;
  request.jobId = job.id;
  request.rangeId = range.id;
  request.engine = engine->info().name;
  request.backend = "cuda";
  request.workspace = workspace;
  request.command = engine->buildCommand(launchRequest);
  request.echoOutput = true;

  return request;
}

} // namespace openpuzzle
