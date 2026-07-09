#include "openpuzzle/runtime/RuntimeDispatcher.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include "openpuzzle/runtime/ExecutionRepository.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <stdexcept>

namespace openpuzzle {

RuntimeDispatcher::RuntimeDispatcher(Database& database)
    : database_(database) {}

bool RuntimeDispatcher::dispatch(const SchedulerDecision& decision) const {
  if (!decision.shouldDispatch) {
    return false;
  }

  auto job = database_.getJob(decision.jobId);

  if (!job) {
    return false;
  }

  auto range = database_.getRange(job->rangeId);

  if (!range) {
    return false;
  }

  auto worker = database_.getWorker(decision.workerId);

  if (!worker) {
    return false;
  }

  return true;
}


StartExecutionRequest RuntimeDispatcher::prepare(
    const SchedulerDecision& decision) const {
  if (!dispatch(decision)) {
    throw std::runtime_error("Invalid scheduler decision");
  }

  auto job = database_.getJob(decision.jobId);

  if (!job) {
    throw std::runtime_error("Job not found");
  }

  auto range = database_.getRange(job->rangeId);

  if (!range) {
    throw std::runtime_error("Range not found");
  }

  auto puzzle = database_.getPuzzleById(job->puzzleId);

  if (!puzzle) {
    throw std::runtime_error("Puzzle not found");
  }

  auto bitcrack = ToolManager::bitcrackCudaPath();

  if (!bitcrack) {
    throw std::runtime_error("BitCrack CUDA executable not configured");
  }

  WorkspaceManager workspaceManager(
      std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") /
      ".local/share/OpenPuzzle");

  auto workspace = workspaceManager.createJobWorkspace(job->id).string();
  auto outputFile = workspaceManager.foundFile(job->id).string();
  auto logFile = workspaceManager.engineLog(job->id, "bitcrack").string();

  EngineLaunchRequest launchRequest;
  launchRequest.puzzle = *puzzle;
  launchRequest.range = *range;
  launchRequest.device = 0;
  launchRequest.blocks = 256;
  launchRequest.threads = 256;
  launchRequest.points = 1024;
  launchRequest.workspace = workspace;
  launchRequest.outputFile = outputFile;
  launchRequest.logFile = logFile;

  EngineManager engineManager;
  auto engine = engineManager.create("bitcrack", *bitcrack);

  if (!engine) {
    throw std::runtime_error("Could not create BitCrack engine");
  }

  ExecutionRepository executionRepository(database_);

  int executionId = executionRepository.create(
      job->id,
      workspace,
      engine->buildCommand(launchRequest),
      "running");

  if (executionId <= 0) {
    throw std::runtime_error("Could not create execution");
  }

  database_.updateJobState(job->id, JobState::Running);
  database_.updateRangeStatus(range->id, RangeStatus::Running);

  StartExecutionRequest request;
  request.executionId = executionId;
  request.puzzleId = puzzle->id;
  request.jobId = job->id;
  request.rangeId = range->id;
  request.engine = engine->info().name;
  request.backend = "cuda";
  request.workspace = workspace;
  request.command = engine->buildCommand(launchRequest);
  request.echoOutput = true;

  return request;
}

} // namespace openpuzzle
