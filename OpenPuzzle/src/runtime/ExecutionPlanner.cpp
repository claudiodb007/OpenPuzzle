#include "openpuzzle/runtime/ExecutionPlanner.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/scheduler/CapabilityScheduler.hpp"

namespace openpuzzle {

ExecutionPlanner::ExecutionPlanner(
    Database& database,
    WorkerAgentRegistry& workers)
    : database_(database),
      workers_(workers) {}

std::optional<ExecutionPlan>
ExecutionPlanner::plan() {

  auto job =
      database_.nextReservedJob();

  if (!job)
    return std::nullopt;

  CapabilityScheduler scheduler(
      workers_);

  auto capability =
      scheduler.select(
          "BitCrack",
          "CUDA");

  if (!capability)
    return std::nullopt;

  ExecutionPlan plan;

  plan.workerId =
      capability->workerId;

  plan.jobId =
      job->id;

  plan.engine =
      capability->engine;

  plan.backend =
      capability->backend;

  plan.capability =
      capability->capability;

  plan.expectedSpeedMkeys =
      capability->expectedSpeedMkeys;

  plan.valid = true;

  return plan;
}

}
