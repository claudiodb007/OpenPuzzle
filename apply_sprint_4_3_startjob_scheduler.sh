#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/include/openpuzzle/core/Scheduler.hpp")
txt = p.read_text()

if "int executionId = 0;" not in txt:
    txt = txt.replace(
        "int rangeId = 0;\n  int exitCode = -1;",
        "int rangeId = 0;\n  int executionId = 0;\n  int exitCode = -1;"
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

if "schedulerResult.executionId = executionId;" not in txt:
    txt = txt.replace(
        """  if (executionId <= 0) {
    schedulerResult.success = false;
    schedulerResult.exitCode = -1;
    return schedulerResult;
  }""",
        """  if (executionId <= 0) {
    schedulerResult.success = false;
    schedulerResult.exitCode = -1;
    return schedulerResult;
  }

  schedulerResult.executionId = executionId;"""
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Application.cpp")
txt = p.read_text()

if '#include "openpuzzle/core/Scheduler.hpp"' not in txt:
    txt = txt.replace(
        '#include "openpuzzle/core/ProcessRunner.hpp"',
        '#include "openpuzzle/core/ProcessRunner.hpp"\n#include "openpuzzle/core/Scheduler.hpp"'
    )

start = txt.find("int Application::cmdStartJob")
if start == -1:
    raise SystemExit("Não encontrei cmdStartJob")

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
    raise SystemExit("Não encontrei fim de cmdStartJob")

new_func = r'''int Application::cmdStartJob(const std::vector<std::string> &a) {
  int n = getIntArg(a, "--puzzle", 71), jid = getIntArg(a, "--job", 0),
      b = getIntArg(a, "--blocks", 256), t = getIntArg(a, "--threads", 256),
      pt = getIntArg(a, "--points", 1024);

  bool dry = hasArg(a, "--dry-run");

  Database db;
  if (!ensureDb(db))
    return 1;

  auto puzzle = db.getPuzzleByNumber(n);
  auto job = db.getJob(jid);

  if (!puzzle || !job)
    throw std::runtime_error("Puzzle/job not found");

  auto range = db.getRange(job->rangeId);
  auto bitcrack = ToolManager::bitcrackPath();

  if (!range || !bitcrack)
    throw std::runtime_error("Range/BitCrack not found");

  Scheduler scheduler;

  auto workspace = scheduler.workspaceForJob(jid);
  auto output = (fs::path(workspace) / "found.txt").string();
  auto log = (fs::path(workspace) / "bitcrack.log").string();

  auto command =
      scheduler.buildBitCrackCommand(*bitcrack, *puzzle, *range,
                                     GpuManager::selectedGpu(), b, t, pt,
                                     output) +
      " 2>&1 | tee -a " + log;

  auto context = scheduler.buildExecutionContext(
      0, puzzle->id, job->id, range->id, "BitCrack", workspace, command, true);

  ExecutionManager executionManager;
  auto result =
      scheduler.runExistingJob(db, *job, *range, context, executionManager, dry);

  std::cout << "Workspace............ " << workspace << "\n";
  std::cout << "Execution ID......... " << result.executionId << "\n";
  std::cout << "Command.............. " << command << "\n";

  if (dry) {
    std::cout << "Dry run only.\n";
  }

  return result.exitCode;
}'''

txt = txt[:start] + new_func + txt[end:]
p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh
