#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"
#include "openpuzzle/workers/WorkerLifecycle.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace openpuzzle;

static int createExecution(
    Database& database,
    const std::string& workspace) {
  PuzzleRecord puzzle;
  puzzle.number = 71;
  puzzle.name = "Puzzle 71";
  puzzle.address = "test";
  puzzle.rangeStart = "400000000000000000";
  puzzle.rangeEnd = "7FFFFFFFFFFFFFFFFF";
  puzzle.sharing = "private";

  assert(database.upsertPuzzle(puzzle));

  auto stored =
      database.getPuzzleByNumber(71);

  assert(stored);

  RangeRecord range;
  range.puzzleId = stored->id;
  range.startKey = "400000000000000000";
  range.endKey = "4000000000000000FF";
  range.blockBits = 8;
  range.status = RangeStatus::Running;

  const int rangeId =
      database.insertRange(range);

  assert(rangeId > 0);

  JobRecord job;
  job.puzzleId = stored->id;
  job.rangeId = rangeId;
  job.state = JobState::Running;

  const int jobId =
      database.insertJob(job);

  assert(jobId > 0);

  const int executionId =
      database.insertExecution(
          jobId,
          workspace,
          "exit 0",
          "running");

  assert(executionId > 0);

  return executionId;
}

int main() {
  Database database;

  assert(database.open(":memory:"));
  assert(database.createSchema());

  WorkerRecord record;
  record.machine = "local-node";
  record.gpuName = "RTX 4070 Super";
  record.backend = "CUDA";
  record.engine = "BitCrack";
  record.status = "idle";

  const int workerId =
      database.upsertWorker(record);

  assert(workerId > 0);

  WorkerAgentInfo info;
  info.workerId = workerId;
  info.machine = record.machine;
  info.gpuName = record.gpuName;
  info.backend = record.backend;
  info.engine = record.engine;
  info.state = WorkerAgentState::Idle;

  WorkerAgentRegistry workers;

  assert(workers.add(WorkerAgent(info)));

  auto* worker = workers.find(workerId);
  assert(worker);

  auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-worker-lifecycle-" +
       std::to_string(getpid()));

  std::filesystem::remove_all(workspace);
  std::filesystem::create_directories(workspace);

  const int executionId =
      createExecution(
          database,
          workspace.string());

  StartExecutionRequest request;
  request.executionId = executionId;
  request.workspace = workspace.string();
  request.command = "exit 0";

  BackgroundExecutionLauncher launcher;

  auto handle =
      worker->execute(
          launcher,
          request);

  assert(handle.pid > 0);
  assert(worker->busy());
  assert(worker->hasExecution());

  assert(database.updateWorkerStatus(
      workerId,
      "running"));

  std::this_thread::sleep_for(
      std::chrono::milliseconds(250));

  WorkerLifecycle lifecycle(
      database,
      workers);

  auto summary =
      lifecycle.monitorExecutions();

  assert(summary.running == 1);
  assert(summary.finished == 1);
  assert(summary.failed == 0);

  auto execution =
      database.getExecution(executionId);

  assert(execution);
  assert(
      execution->status ==
      ExecutionRecordStatus::Finished);

  assert(worker->idle());
  assert(!worker->hasExecution());

  auto updatedWorker =
      database.getWorker(workerId);

  assert(updatedWorker);
  assert(updatedWorker->status == "idle");

  lifecycle.refreshHeartbeats();

  std::filesystem::remove_all(workspace);

  std::cout
      << "WorkerLifecycleTests passed\n";

  return 0;
}
