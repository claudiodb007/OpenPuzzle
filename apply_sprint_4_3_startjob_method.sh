#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/include/openpuzzle/core/Scheduler.hpp")
txt = p.read_text()

if "startJob(Database" not in txt:
    txt = txt.replace(
        "SchedulerResult runExistingJob(Database &db, const JobRecord &job,",
        """SchedulerResult startJob(Database &db, int puzzleNumber, int jobId,
                           const std::string &bitcrackPath, int device,
                           int blocks, int threads, int points,
                           bool dryRun) const;

  SchedulerResult runExistingJob(Database &db, const JobRecord &job,"""
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

if "Scheduler::startJob" not in txt:
    txt = txt.replace(
        "SchedulerResult Scheduler::runExistingJob(",
        r'''SchedulerResult Scheduler::startJob(
    Database &db, int puzzleNumber, int jobId, const std::string &bitcrackPath,
    int device, int blocks, int threads, int points, bool dryRun) const {
  auto puzzle = db.getPuzzleByNumber(puzzleNumber);
  auto job = db.getJob(jobId);

  if (!puzzle || !job) {
    SchedulerResult result;
    result.success = false;
    result.jobId = jobId;
    result.exitCode = -1;
    return result;
  }

  auto range = db.getRange(job->rangeId);

  if (!range) {
    SchedulerResult result;
    result.success = false;
    result.jobId = jobId;
    result.exitCode = -1;
    return result;
  }

  auto workspace = workspaceForJob(jobId);
  auto outputFile =
      (std::filesystem::path(workspace) / "found.txt").string();
  auto logFile =
      (std::filesystem::path(workspace) / "bitcrack.log").string();

  auto command = buildBitCrackCommand(bitcrackPath, *puzzle, *range, device,
                                      blocks, threads, points, outputFile) +
                 " 2>&1 | tee -a " + logFile;

  auto context = buildExecutionContext(0, puzzle->id, job->id, range->id,
                                       "BitCrack", workspace, command, true);

  ExecutionManager executionManager;
  return runExistingJob(db, *job, *range, context, executionManager, dryRun);
}

SchedulerResult Scheduler::runExistingJob('''
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Application.cpp")
txt = p.read_text()

start = txt.find("int Application::cmdStartJob")
if start == -1:
    raise SystemExit("cmdStartJob não encontrado")

brace = txt.find("{", start)
depth = 0
end = None

for i in range(brace, len(txt)):
    if txt[i] == "{":
        depth += 1
    elif txt[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break

if end is None:
    raise SystemExit("fim de cmdStartJob não encontrado")

new_func = r'''int Application::cmdStartJob(const std::vector<std::string> &a) {
  int n = getIntArg(a, "--puzzle", 71), jid = getIntArg(a, "--job", 0),
      b = getIntArg(a, "--blocks", 256), t = getIntArg(a, "--threads", 256),
      pt = getIntArg(a, "--points", 1024);

  bool dry = hasArg(a, "--dry-run");

  Database db;
  if (!ensureDb(db))
    return 1;

  auto bitcrack = ToolManager::bitcrackPath();

  if (!bitcrack)
    throw std::runtime_error("BitCrack not found");

  Scheduler scheduler;

  auto result = scheduler.startJob(db, n, jid, *bitcrack,
                                   GpuManager::selectedGpu(), b, t, pt, dry);

  std::cout << "Job.................. " << result.jobId << "\n";
  std::cout << "Range................ " << result.rangeId << "\n";
  std::cout << "Execution ID......... " << result.executionId << "\n";
  std::cout << "Exit code............ " << result.exitCode << "\n";

  if (dry) {
    std::cout << "Dry run only.\n";
  }

  return result.exitCode;
}'''

txt = txt[:start] + new_func + txt[end:]
p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/SchedulerStartJobTests.cpp <<'CPP'
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
    puzzle.address = "test-address";
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

    Scheduler scheduler;

    auto result = scheduler.startJob(
        db,
        71,
        job.id,
        "printf",
        0,
        256,
        256,
        1024,
        true
    );

    if (!result.success) return 7;
    if (result.jobId != job.id) return 8;
    if (result.rangeId != range.id) return 9;
    if (result.executionId <= 0) return 10;
    if (result.exitCode != 0) return 11;

    return 0;
}
CPP

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "SchedulerStartJobTests" not in txt:
    txt += """

add_executable(SchedulerStartJobTests
    core/SchedulerStartJobTests.cpp
)

target_link_libraries(SchedulerStartJobTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerStartJobTests
    COMMAND SchedulerStartJobTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh
