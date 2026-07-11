#include "openpuzzle/runtime/RuntimeDispatcher.hpp"

#include "openpuzzle/core/WorkspaceManager.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/dispatcher/ProfileSelector.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionRepository.hpp"
#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace openpuzzle {

RuntimeDispatcher::RuntimeDispatcher(
    Database& database,
    WorkerAgentRegistry& workers,
    EngineManager& engineManager)
    : database_(database),
      workers_(workers),
      engineManager_(engineManager) {}

ExecutionPlan RuntimeDispatcher::planFromDecision(
    const SchedulerDecision& decision) const {
  ExecutionPlan plan;

  if (!decision.shouldDispatch) {
    return plan;
  }

  auto* worker =
      workers_.find(decision.workerId);

  if (!worker || !worker->idle()) {
    return plan;
  }

  WorkerEngineCapability capability;

  if (const auto* registered =
          worker->bestCapability(
              worker->info().engine,
              worker->info().backend)) {
    capability = *registered;
  } else {
    capability.engine =
        worker->info().engine;

    capability.backend =
        worker->info().backend;

    capability.device = 0;

    capability.benchmarkSpeedMkeys =
        worker->info().speedMkeys;
  }

  WorkerRecord workerRecord =
      worker->toRecord();

  ProfileSelector profileSelector(
      database_);

  auto profile =
      profileSelector.select(
          workerRecord);

  if (profile) {
    capability.blocks =
        profile->blocks;

    capability.threads =
        profile->threads;

    capability.points =
        profile->points;

    if (capability.benchmarkSpeedMkeys <= 0.0) {
      capability.benchmarkSpeedMkeys =
          profile->averageSpeed;
    }
  }

  plan.workerId =
      decision.workerId;

  plan.jobId =
      decision.jobId;

  plan.engine =
      capability.engine;

  plan.backend =
      capability.backend;

  plan.capability =
      capability;

  plan.expectedSpeedMkeys =
      capability.benchmarkSpeedMkeys;

  plan.valid =
      capability.available &&
      capability.hasLaunchProfile();

  return plan;
}

bool RuntimeDispatcher::dispatch(
    const ExecutionPlan& plan) const {
  if (!plan.valid ||
      plan.jobId <= 0 ||
      plan.workerId <= 0) {
    return false;
  }

  auto job =
      database_.getJob(plan.jobId);

  if (!job ||
      job->state != JobState::Reserved) {
    return false;
  }

  auto range =
      database_.getRange(job->rangeId);

  if (!range ||
      range->status != RangeStatus::Reserved) {
    return false;
  }

  auto* worker =
      workers_.find(plan.workerId);

  if (!worker ||
      !worker->idle()) {
    return false;
  }

  if (plan.engine.empty() ||
      plan.backend.empty()) {
    return false;
  }

  if (!plan.capability.available ||
      !plan.capability.hasLaunchProfile()) {
    return false;
  }

  if (!plan.capability.matches(
          plan.engine,
          plan.backend)) {
    return false;
  }

  return true;
}

StartExecutionRequest RuntimeDispatcher::prepare(
    const ExecutionPlan& plan) const {
  if (!dispatch(plan)) {
    throw std::runtime_error(
        "Invalid execution plan");
  }

  auto job =
      database_.getJob(plan.jobId);

  if (!job) {
    throw std::runtime_error(
        "Job not found");
  }

  auto range =
      database_.getRange(job->rangeId);

  if (!range) {
    throw std::runtime_error(
        "Range not found");
  }

  auto puzzle =
      database_.getPuzzleById(
          job->puzzleId);

  if (!puzzle) {
    throw std::runtime_error(
        "Puzzle not found");
  }

  auto* worker =
      workers_.find(plan.workerId);

  if (!worker) {
    throw std::runtime_error(
        "Worker agent not found");
  }

  const auto& capability =
      plan.capability;

  auto executable =
      engineManager_.resolveExecutable(
          plan.engine,
          plan.backend);

  if (!executable) {
    throw std::runtime_error(
        "Search engine executable not available: " +
        plan.engine +
        " (" +
        plan.backend +
        ")");
  }

  WorkspaceManager workspaceManager(
      std::filesystem::path(
          std::getenv("HOME")
              ? std::getenv("HOME")
              : ".") /
      ".local/share/OpenPuzzle");

  const auto workspace =
      workspaceManager
          .createJobWorkspace(job->id)
          .string();

  ExecutionRequestBuilder builder(
      engineManager_);

  auto request =
      builder.build(
          *puzzle,
          *range,
          *job,
          capability,
          *executable,
          workspace);

  ExecutionRepository repository(
      database_);

  const int executionId =
      repository.create(
          job->id,
          workspace,
          request.command,
          "running");

  if (executionId <= 0) {
    throw std::runtime_error(
        "Could not create execution");
  }

  if (!database_.updateJobState(
          job->id,
          JobState::Running)) {
    throw std::runtime_error(
        "Could not update job state");
  }

  if (!database_.updateRangeStatus(
          range->id,
          RangeStatus::Running)) {
    throw std::runtime_error(
        "Could not update range state");
  }

  request.executionId =
      executionId;

  return request;
}

ExecutionResult RuntimeDispatcher::dispatchAndLaunch(
    const ExecutionPlan& plan) const {
  auto request =
      prepare(plan);

  auto* worker =
      workers_.find(plan.workerId);

  if (!worker) {
    throw std::runtime_error(
        "Worker agent not found");
  }

  BackgroundExecutionLauncher launcher;

  auto handle =
      worker->execute(
          launcher,
          request);

  if (!database_.updateWorkerStatus(
          worker->info().workerId,
          WorkerAgent::stateToString(
              worker->state()))) {
    throw std::runtime_error(
        "Could not update worker state");
  }

  ExecutionResult result;

  result.success =
      handle.pid > 0;

  result.exitCode =
      result.success
          ? 0
          : -1;

  return result;
}

bool RuntimeDispatcher::dispatch(
    const SchedulerDecision& decision) const {
  return dispatch(
      planFromDecision(decision));
}

StartExecutionRequest RuntimeDispatcher::prepare(
    const SchedulerDecision& decision) const {
  return prepare(
      planFromDecision(decision));
}

ExecutionResult RuntimeDispatcher::dispatchAndLaunch(
    const SchedulerDecision& decision) const {
  return dispatchAndLaunch(
      planFromDecision(decision));
}

} // namespace openpuzzle
