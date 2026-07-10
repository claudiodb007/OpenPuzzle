#include "openpuzzle/runtime/RuntimeDispatcher.hpp"

#include "openpuzzle/core/WorkspaceManager.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/dispatcher/ProfileSelector.hpp"
#include "openpuzzle/runtime/ExecutionRepository.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace openpuzzle {

RuntimeDispatcher::RuntimeDispatcher(
    Database& database,
    WorkerAgentRegistry& workers)
    : database_(database),
      workers_(workers) {}

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

  auto* worker = workers_.find(decision.workerId);

  if (!worker || !worker->idle()) {
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

  auto* worker = workers_.find(decision.workerId);

  if (!worker) {
    throw std::runtime_error("Worker agent not found");
  }

  WorkerEngineCapability capability;

  if (const auto* registered =
          worker->bestCapability(
              worker->info().engine,
              worker->info().backend)) {
    capability = *registered;
  } else {
    capability.engine = worker->info().engine;
    capability.backend = worker->info().backend;
    capability.device = 0;
    capability.benchmarkSpeedMkeys =
        worker->info().speedMkeys;
  }

  WorkerRecord workerRecord = worker->toRecord();

  ProfileSelector profileSelector(database_);
  auto profile = profileSelector.select(workerRecord);

  if (profile) {
    capability.blocks = profile->blocks;
    capability.threads = profile->threads;
    capability.points = profile->points;

    if (capability.benchmarkSpeedMkeys <= 0.0) {
      capability.benchmarkSpeedMkeys =
          profile->averageSpeed;
    }
  }

  if (!capability.hasLaunchProfile()) {
    throw std::runtime_error(
        "No GPU launch profile available for worker");
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
      capability,
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

  auto* worker = workers_.find(decision.workerId);

  if (!worker) {
    throw std::runtime_error("Worker agent not found");
  }

  BackgroundExecutionLauncher launcher;
  auto handle = worker->execute(launcher, request);

  if (!database_.updateWorkerStatus(
          worker->info().workerId,
          WorkerAgent::stateToString(worker->state()))) {
    throw std::runtime_error("Could not update worker state");
  }

  ExecutionResult result;
  result.success = handle.pid > 0;
  result.exitCode = result.success ? 0 : -1;

  return result;
}

} // namespace openpuzzle
