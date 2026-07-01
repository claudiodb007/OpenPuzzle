#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/include/openpuzzle/core/Scheduler.hpp")
txt = p.read_text()

if "buildExecutionContext" not in txt:
    txt = txt.replace(
        "SchedulerResult runExecution(const ExecutionContext &context,\n                               const ExecutionManager &executionManager) const;",
        """ExecutionContext buildExecutionContext(int executionId, int puzzleId,
                                         int jobId, int rangeId,
                                         const std::string &engine,
                                         const std::string &workspace,
                                         const std::string &command,
                                         bool echoOutput) const;

  SchedulerResult runExecution(const ExecutionContext &context,
                               const ExecutionManager &executionManager) const;"""
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

if "Scheduler::buildExecutionContext" not in txt:
    txt = txt.replace(
        "SchedulerResult\nScheduler::runExecution",
        """ExecutionContext Scheduler::buildExecutionContext(
    int executionId, int puzzleId, int jobId, int rangeId,
    const std::string &engine, const std::string &workspace,
    const std::string &command, bool echoOutput) const {
  ExecutionContext context;

  context.executionId = executionId;
  context.puzzleId = puzzleId;
  context.jobId = jobId;
  context.rangeId = rangeId;
  context.engine = engine;
  context.workspace = workspace;
  context.command = command;
  context.echoOutput = echoOutput;

  return context;
}

SchedulerResult
Scheduler::runExecution"""
    )

p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/SchedulerContextBuilderTests.cpp <<'CPP'
#include "openpuzzle/core/Scheduler.hpp"

using namespace openpuzzle;

int main()
{
    Scheduler scheduler;

    auto ctx = scheduler.buildExecutionContext(
        7,
        71,
        42,
        1001,
        "BitCrack",
        "/tmp/openpuzzle/jobs/00000042",
        "printf test",
        false
    );

    if (ctx.executionId != 7) return 1;
    if (ctx.puzzleId != 71) return 2;
    if (ctx.jobId != 42) return 3;
    if (ctx.rangeId != 1001) return 4;
    if (ctx.engine != "BitCrack") return 5;
    if (ctx.workspace != "/tmp/openpuzzle/jobs/00000042") return 6;
    if (ctx.command != "printf test") return 7;
    if (ctx.echoOutput != false) return 8;

    return 0;
}
CPP

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "SchedulerContextBuilderTests" not in txt:
    txt += """

add_executable(SchedulerContextBuilderTests
    core/SchedulerContextBuilderTests.cpp
)

target_link_libraries(SchedulerContextBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerContextBuilderTests
    COMMAND SchedulerContextBuilderTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh
