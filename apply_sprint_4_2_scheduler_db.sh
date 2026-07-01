#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/include/openpuzzle/core/Scheduler.hpp")
txt = p.read_text()

if '#include "openpuzzle/database/Database.hpp"' not in txt:
    txt = txt.replace(
        '#include "openpuzzle/core/ExecutionResult.hpp"',
        '#include "openpuzzle/core/ExecutionResult.hpp"\n#include "openpuzzle/database/Database.hpp"'
    )

if "runExistingJob" not in txt:
    txt = txt.replace(
        "SchedulerResult runExecution(const ExecutionContext &context,\n                               const ExecutionManager &executionManager) const;",
        """SchedulerResult runExecution(const ExecutionContext &context,
                               const ExecutionManager &executionManager) const;

  SchedulerResult runExistingJob(Database &db, const JobRecord &job,
                                 const RangeRecord &range,
                                 const ExecutionContext &context,
                                 const ExecutionManager &executionManager,
                                 bool dryRun) const;"""
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

if "Scheduler::runExistingJob" not in txt:
    txt = txt.replace(
        "\n} // namespace openpuzzle",
        r'''

SchedulerResult Scheduler::runExistingJob(
    Database &db, const JobRecord &job, const RangeRecord &range,
    const ExecutionContext &context, const ExecutionManager &executionManager,
    bool dryRun) const {
  SchedulerResult schedulerResult;
  schedulerResult.jobId = job.id;
  schedulerResult.rangeId = range.id;

  int executionId = db.insertExecution(
      job.id, context.workspace, context.command, dryRun ? "dry-run" : "running");

  if (executionId <= 0) {
    schedulerResult.success = false;
    schedulerResult.exitCode = -1;
    return schedulerResult;
  }

  if (dryRun) {
    schedulerResult.success = true;
    schedulerResult.exitCode = 0;
    return schedulerResult;
  }

  db.updateJobState(job.id, JobState::Running);
  db.updateRangeStatus(range.id, RangeStatus::Running);

  auto executionResult = executionManager.run(context);

  db.finishExecution(executionId,
                     executionResult.success ? "finished" : "failed",
                     executionResult.exitCode);

  if (executionResult.success) {
    db.updateJobState(job.id, JobState::Completed);
    db.updateRangeStatus(range.id, RangeStatus::Completed);
  } else {
    db.updateJobState(job.id, JobState::Failed);
    db.updateRangeStatus(range.id, RangeStatus::Failed);
  }

  schedulerResult.success = executionResult.success;
  schedulerResult.exitCode = executionResult.exitCode;

  return schedulerResult;
}

} // namespace openpuzzle'''
    )

p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/SchedulerDatabaseTests.cpp <<'CPP'
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"

using namespace openpuzzle;

int main()
{
    Database db;

    if (!db.open(":memory:")) return 1;
    if (!db.createSchema()) return 2;

    PuzzleRecord puzzle;
    puzzle.number = 71;
    puzzle.name = "Bitcoin Puzzle 71";
    puzzle.address = "test";
    puzzle.rangeStart = "400000000000000000";
    puzzle.rangeEnd = "7FFFFFFFFFFFFFFFFF";
    puzzle.reward = 0.0;
    puzzle.sharing = "private";

    if (!db.upsertPuzzle(puzzle)) return 3;

    auto savedPuzzle = db.getPuzzleByNumber(71);
    if (!savedPuzzle) return 4;

    RangeRecord range;
    range.puzzleId = savedPuzzle->id;
    range.startKey = "400000000000000000";
    range.endKey = "4000000000000000FF";
    range.blockBits = 8;
    range.status = RangeStatus::Reserved;
    range.id = db.insertRange(range);

    if (range.id <= 0) return 5;

    JobRecord job;
    job.puzzleId = savedPuzzle->id;
    job.rangeId = range.id;
    job.state = JobState::Reserved;
    job.id = db.insertJob(job);

    if (job.id <= 0) return 6;

    ExecutionContext ctx;
    ctx.executionId = 1;
    ctx.puzzleId = savedPuzzle->id;
    ctx.jobId = job.id;
    ctx.rangeId = range.id;
    ctx.engine = "Test";
    ctx.command = "printf '[Info] 1700.00 MKey/s\\nFinished\\n'";
    ctx.echoOutput = false;

    Scheduler scheduler;
    ExecutionManager executionManager;

    auto result = scheduler.runExistingJob(db, job, range, ctx, executionManager, false);

    if (!result.success) return 7;
    if (result.jobId != job.id) return 8;
    if (result.rangeId != range.id) return 9;
    if (result.exitCode != 0) return 10;

    auto updatedJob = db.getJob(job.id);
    if (!updatedJob) return 11;
    if (updatedJob->state != JobState::Completed) return 12;

    auto updatedRange = db.getRange(range.id);
    if (!updatedRange) return 13;
    if (updatedRange->status != RangeStatus::Completed) return 14;

    return 0;
}
CPP

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "SchedulerDatabaseTests" not in txt:
    txt += """

add_executable(SchedulerDatabaseTests
    core/SchedulerDatabaseTests.cpp
)

target_link_libraries(SchedulerDatabaseTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerDatabaseTests
    COMMAND SchedulerDatabaseTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh
