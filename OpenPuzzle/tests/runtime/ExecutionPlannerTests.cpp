#include "openpuzzle/runtime/ExecutionPlanner.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

static WorkerEngineCapability capability(
    double speed) {
  WorkerEngineCapability result;

  result.engine = "BitCrack";
  result.backend = "CUDA";

  result.device = 0;
  result.blocks = 256;
  result.threads = 256;
  result.points = 1024;

  result.available = true;
  result.benchmarkSpeedMkeys = speed;

  return result;
}

int main() {
  Database database;

  assert(database.open(":memory:"));
  assert(database.createSchema());

  PuzzleRecord puzzle;
  puzzle.number = 71;
  puzzle.name = "Puzzle 71";
  puzzle.address =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
  puzzle.rangeStart =
      "400000000000000000";
  puzzle.rangeEnd =
      "7FFFFFFFFFFFFFFFFF";
  puzzle.sharing = "private";

  assert(database.upsertPuzzle(puzzle));

  auto storedPuzzle =
      database.getPuzzleByNumber(71);

  assert(storedPuzzle);

  RangeRecord range;
  range.puzzleId = storedPuzzle->id;
  range.startKey =
      "400000000000000000";
  range.endKey =
      "40000000FFFFFFFFFF";
  range.blockBits = 40;
  range.status = RangeStatus::Reserved;

  const int rangeId =
      database.insertRange(range);

  assert(rangeId > 0);

  JobRecord job;
  job.puzzleId = storedPuzzle->id;
  job.rangeId = rangeId;
  job.state = JobState::Reserved;

  const int jobId =
      database.insertJob(job);

  assert(jobId > 0);

  WorkerAgentRegistry workers;

  WorkerAgentInfo slowInfo;
  slowInfo.workerId = 5;
  slowInfo.machine = "slow-node";
  slowInfo.gpuName = "RTX 3060";
  slowInfo.engine = "BitCrack";
  slowInfo.backend = "CUDA";
  slowInfo.state = WorkerAgentState::Idle;
  slowInfo.capabilities.push_back(
      capability(900.0));

  assert(workers.add(
      WorkerAgent(slowInfo)));

  WorkerAgentInfo fastInfo;
  fastInfo.workerId = 7;
  fastInfo.machine = "fast-node";
  fastInfo.gpuName = "RTX 4070 Super";
  fastInfo.engine = "BitCrack";
  fastInfo.backend = "CUDA";
  fastInfo.state = WorkerAgentState::Idle;
  fastInfo.capabilities.push_back(
      capability(1550.0));

  assert(workers.add(
      WorkerAgent(fastInfo)));

  ExecutionPlanner planner(
      database,
      workers);

  auto plan = planner.plan();

  assert(plan);
  assert(plan->valid);

  assert(plan->workerId == 7);
  assert(plan->jobId == jobId);

  assert(plan->engine == "BitCrack");
  assert(plan->backend == "CUDA");

  assert(
      plan->expectedSpeedMkeys ==
      1550.0);

  assert(
      plan->capability.blocks ==
      256);

  assert(
      plan->capability.threads ==
      256);

  assert(
      plan->capability.points ==
      1024);

  std::cout
      << "ExecutionPlannerTests passed\n";

  return 0;
}
