#include "openpuzzle/runtime/RuntimeDispatcher.hpp"

#include "openpuzzle/core/WorkspaceManager.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/runtime/ExecutionRepository.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <cstdlib>
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

  ExecutionRequestBuilder builder;

  auto request = builder.build(
      *puzzle,
      *range,
      *job,
      *bitcrack,
      workspace);

  ExecutionRepository executionRepository(database_);

  int executionId = executionRepository.create(
      job->id,
      workspace,
      request.command,
      "running");

  if (executionId <= 0) {
    throw std::runtime_error("Could not create execution");
  }

  database_.updateJobState(job->id, JobState::Running);
  database_.updateRangeStatus(range->id, RangeStatus::Running);

  request.executionId = executionId;

  return request;
}



ExecutionResult RuntimeDispatcher::dispatchAndLaunch(
    const SchedulerDecision& decision) const {

  auto request = prepare(decision);

  BackgroundExecutionLauncher launcher;
  auto handle = launcher.start(request);

  ExecutionResult result;
  result.success = true;
  result.exitCode = 0;

  return result;
}

} // namespace openpuzzle
