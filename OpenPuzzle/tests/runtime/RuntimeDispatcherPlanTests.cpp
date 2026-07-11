#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/runtime/ExecutionPlan.hpp"
#include "openpuzzle/runtime/RuntimeDispatcher.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

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

  WorkerEngineCapability capability;
  capability.engine = "BitCrack";
  capability.backend = "CUDA";
  capability.device = 1;
  capability.blocks = 256;
  capability.threads = 512;
  capability.points = 2048;
  capability.benchmarkSpeedMkeys = 1500.0;

  WorkerAgentInfo workerInfo;
  workerInfo.workerId = 7;
  workerInfo.machine = "plan-node";
  workerInfo.gpuName = "RTX 4070 Super";
  workerInfo.engine = "BitCrack";
  workerInfo.backend = "CUDA";
  workerInfo.state = WorkerAgentState::Idle;
  workerInfo.capabilities.push_back(
      capability);

  WorkerAgentRegistry workers;

  assert(workers.add(
      WorkerAgent(workerInfo)));

  EngineManager engines;

  RuntimeDispatcher dispatcher(
      database,
      workers,
      engines);

  ExecutionPlan plan;
  plan.workerId = 7;
  plan.jobId = jobId;
  plan.engine = "BitCrack";
  plan.backend = "CUDA";
  plan.capability = capability;
  plan.expectedSpeedMkeys = 1500.0;
  plan.valid = true;

  assert(dispatcher.dispatch(plan));

  auto invalidWorker = plan;
  invalidWorker.workerId = 99;

  assert(!dispatcher.dispatch(
      invalidWorker));

  auto invalidEngine = plan;
  invalidEngine.engine = "KeyHunt";

  assert(!dispatcher.dispatch(
      invalidEngine));

  auto invalidProfile = plan;
  invalidProfile.capability.blocks = 0;

  assert(!dispatcher.dispatch(
      invalidProfile));

  auto invalidPlan = plan;
  invalidPlan.valid = false;

  assert(!dispatcher.dispatch(
      invalidPlan));

  std::cout
      << "RuntimeDispatcherPlanTests passed\n";

  return 0;
}
