#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/runtime/ExecutionProcessMonitor.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

static int createRunningExecution(Database& db,
                                  const std::string& workspace) {
  PuzzleRecord puzzle;
  puzzle.number = 71;
  puzzle.name = "Bitcoin Puzzle 71";
  puzzle.address = "test";
  puzzle.rangeStart = "400000000000000000";
  puzzle.rangeEnd = "7FFFFFFFFFFFFFFFFF";
  puzzle.reward = 0.0;
  puzzle.sharing = "private";

  assert(db.upsertPuzzle(puzzle));

  auto savedPuzzle = db.getPuzzleByNumber(71);
  assert(savedPuzzle);

  RangeRecord range;
  range.puzzleId = savedPuzzle->id;
  range.startKey = "400000000000000000";
  range.endKey = "4000000000000000FF";
  range.blockBits = 8;
  range.status = RangeStatus::Running;

  int rangeId = db.insertRange(range);
  assert(rangeId > 0);

  JobRecord job;
  job.puzzleId = savedPuzzle->id;
  job.rangeId = rangeId;
  job.state = JobState::Running;

  int jobId = db.insertJob(job);
  assert(jobId > 0);

  int executionId =
      db.insertExecution(jobId, workspace, "test command", "running");

  assert(executionId > 0);

  return executionId;
}

static std::filesystem::path makeWorkspace(const std::string& name) {
  auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-process-monitor-" + name + "-" + std::to_string(getpid()));

  std::filesystem::remove_all(workspace);
  std::filesystem::create_directories(workspace);

  return workspace;
}

static void writeFile(const std::filesystem::path& path,
                      const std::string& value) {
  std::ofstream out(path);
  out << value;
}

static void testFinished() {
  Database db;
  assert(db.open(":memory:"));
  assert(db.createSchema());

  auto workspace = makeWorkspace("finished");

  int executionId = createRunningExecution(db, workspace.string());

  writeFile(workspace / "process.pid", "999999999\n");
  writeFile(workspace / "exit.code", "0\n");

  ExecutionProcessMonitor monitor(db);
  auto summary = monitor.poll();

  assert(summary.running == 1);
  assert(summary.finished == 1);
  assert(summary.failed == 0);
  assert(summary.cancelled == 0);

  auto execution = db.getExecution(executionId);
  assert(execution);
  assert(execution->status == ExecutionRecordStatus::Finished);
  assert(execution->exitCode == 0);

  std::filesystem::remove_all(workspace);
}

static void testFailed() {
  Database db;
  assert(db.open(":memory:"));
  assert(db.createSchema());

  auto workspace = makeWorkspace("failed");

  int executionId = createRunningExecution(db, workspace.string());

  writeFile(workspace / "process.pid", "999999999\n");
  writeFile(workspace / "exit.code", "5\n");

  ExecutionProcessMonitor monitor(db);
  auto summary = monitor.poll();

  assert(summary.running == 1);
  assert(summary.finished == 0);
  assert(summary.failed == 1);
  assert(summary.cancelled == 0);

  auto execution = db.getExecution(executionId);
  assert(execution);
  assert(execution->status == ExecutionRecordStatus::Failed);
  assert(execution->exitCode == 5);

  std::filesystem::remove_all(workspace);
}

static void testCancelled() {
  Database db;
  assert(db.open(":memory:"));
  assert(db.createSchema());

  auto workspace = makeWorkspace("cancelled");

  int executionId = createRunningExecution(db, workspace.string());

  writeFile(workspace / "process.pid", "999999999\n");
  writeFile(workspace / "exit.code", "-2\n");

  ExecutionProcessMonitor monitor(db);
  auto summary = monitor.poll();

  assert(summary.running == 1);
  assert(summary.finished == 0);
  assert(summary.failed == 0);
  assert(summary.cancelled == 1);

  auto execution = db.getExecution(executionId);
  assert(execution);
  assert(execution->status == ExecutionRecordStatus::Cancelled);
  assert(execution->exitCode == -2);

  std::filesystem::remove_all(workspace);
}

static void testMissingPid() {
  Database db;
  assert(db.open(":memory:"));
  assert(db.createSchema());

  auto workspace = makeWorkspace("missing-pid");

  int executionId = createRunningExecution(db, workspace.string());

  ExecutionProcessMonitor monitor(db);
  auto summary = monitor.poll();

  assert(summary.running == 1);
  assert(summary.missingPid == 1);
  assert(summary.failed == 1);

  auto execution = db.getExecution(executionId);
  assert(execution);
  assert(execution->status == ExecutionRecordStatus::Failed);
  assert(execution->exitCode == -1);

  std::filesystem::remove_all(workspace);
}

int main() {
  testFinished();
  testFailed();
  testCancelled();
  testMissingPid();

  std::cout << "ExecutionProcessMonitorTests passed\n";
  return 0;
}
