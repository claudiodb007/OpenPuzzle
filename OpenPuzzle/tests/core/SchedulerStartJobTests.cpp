#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"

using namespace openpuzzle;

int main() {
  Database db;

  if (!db.open(":memory:"))
    return 1;
  if (!db.createSchema())
    return 2;

  PuzzleRecord puzzle;
  puzzle.number = 71;
  puzzle.name = "Bitcoin Puzzle 71";
  puzzle.address = "test-address";
  puzzle.rangeStart = "400000000000000000";
  puzzle.rangeEnd = "7FFFFFFFFFFFFFFFFF";
  puzzle.reward = 0.0;
  puzzle.sharing = "private";

  if (!db.upsertPuzzle(puzzle))
    return 3;

  auto savedPuzzle = db.getPuzzleByNumber(71);
  if (!savedPuzzle)
    return 4;

  RangeRecord range;
  range.puzzleId = savedPuzzle->id;
  range.startKey = "400000000000000000";
  range.endKey = "4000000000000000FF";
  range.blockBits = 8;
  range.status = RangeStatus::Reserved;
  range.id = db.insertRange(range);

  if (range.id <= 0)
    return 5;

  JobRecord job;
  job.puzzleId = savedPuzzle->id;
  job.rangeId = range.id;
  job.state = JobState::Reserved;
  job.id = db.insertJob(job);

  if (job.id <= 0)
    return 6;

  Scheduler scheduler;

  auto result =
      scheduler.startJob(db, 71, job.id, "printf", 0, 256, 256, 1024, true);

  if (!result.success)
    return 7;
  if (result.jobId != job.id)
    return 8;
  if (result.rangeId != range.id)
    return 9;
  if (result.executionId <= 0)
    return 10;
  if (result.exitCode != 0)
    return 11;

  return 0;
}
