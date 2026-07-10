#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/scheduler/DefaultSchedulingPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

static WorkerRecord makeWorker(
    const char* machine,
    const char* gpu,
    const char* backend,
    const char* engine,
    const char* status,
    double speed) {
  WorkerRecord worker;
  worker.machine = machine;
  worker.gpuName = gpu;
  worker.backend = backend;
  worker.engine = engine;
  worker.status = status;
  worker.speedMkeys = speed;
  return worker;
}

int main() {
  Database db;

  assert(db.open(":memory:"));
  assert(db.createSchema());

  PuzzleRecord puzzle;
  puzzle.number = 71;
  puzzle.name = "Bitcoin Puzzle 71";
  puzzle.address = "test";
  puzzle.rangeStart = "400000000000000000";
  puzzle.rangeEnd = "7FFFFFFFFFFFFFFFFF";
  puzzle.sharing = "private";

  assert(db.upsertPuzzle(puzzle));

  auto storedPuzzle = db.getPuzzleByNumber(71);
  assert(storedPuzzle);

  RangeRecord range;
  range.puzzleId = storedPuzzle->id;
  range.startKey = "400000000000000000";
  range.endKey = "4000000000000000FF";
  range.blockBits = 8;
  range.status = RangeStatus::Reserved;

  int rangeId = db.insertRange(range);
  assert(rangeId > 0);

  JobRecord job;
  job.puzzleId = storedPuzzle->id;
  job.rangeId = rangeId;
  job.state = JobState::Reserved;

  int jobId = db.insertJob(job);
  assert(jobId > 0);

  int slowId = db.upsertWorker(makeWorker(
      "slow-node",
      "RX 5700 XT",
      "OpenCL",
      "BitCrack",
      "idle",
      400.0));

  int fastId = db.upsertWorker(makeWorker(
      "fast-node",
      "RTX 4070 Super",
      "CUDA",
      "BitCrack",
      "idle",
      1350.0));

  int busyId = db.upsertWorker(makeWorker(
      "busy-node",
      "RTX 5080",
      "CUDA",
      "BitCrack",
      "running",
      2500.0));

  assert(slowId > 0);
  assert(fastId > 0);
  assert(busyId > 0);

  DefaultSchedulingPolicy policy;

  auto decision = policy.decide(db);

  assert(decision.shouldDispatch);
  assert(decision.jobId == jobId);
  assert(decision.workerId == fastId);

  assert(db.updateWorkerStatus(fastId, "running"));
  assert(db.updateWorkerStatus(slowId, "running"));

  auto idleDecision = policy.decide(db);

  assert(!idleDecision.shouldDispatch);
  assert(idleDecision.jobId == 0);
  assert(idleDecision.workerId == 0);

  std::cout << "DefaultSchedulingPolicyTests passed\n";
  return 0;
}
