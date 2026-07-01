#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/include/openpuzzle/core/Scheduler.hpp")
txt = p.read_text()

if '#include "openpuzzle/core/ExecutionManager.hpp"' not in txt:
    txt = txt.replace(
        '#include "openpuzzle/core/ExecutionContext.hpp"',
        '#include "openpuzzle/core/ExecutionContext.hpp"\n#include "openpuzzle/core/ExecutionManager.hpp"'
    )

if "runExecution" not in txt:
    txt = txt.replace(
        "SchedulerResult runOnceWithEvents(const ExecutionContext &context,\n                                    const ExecutionResult &executionResult,\n                                    EventBus &bus) const;",
        "SchedulerResult runOnceWithEvents(const ExecutionContext &context,\n                                    const ExecutionResult &executionResult,\n                                    EventBus &bus) const;\n\n  SchedulerResult runExecution(const ExecutionContext &context,\n                               const ExecutionManager &executionManager) const;"
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

if "Scheduler::runExecution" not in txt:
    txt = txt.replace(
        "\n} // namespace openpuzzle",
        r'''

SchedulerResult
Scheduler::runExecution(const ExecutionContext &context,
                        const ExecutionManager &executionManager) const {
  auto executionResult = executionManager.run(context);
  return runOnce(context, executionResult);
}

} // namespace openpuzzle'''
    )

p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/SchedulerExecutionTests.cpp <<'CPP'
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/Scheduler.hpp"

using namespace openpuzzle;

int main()
{
    Scheduler scheduler;
    ExecutionManager executionManager;

    ExecutionContext ctx;
    ctx.executionId = 1;
    ctx.puzzleId = 71;
    ctx.jobId = 42;
    ctx.rangeId = 1001;
    ctx.engine = "Test";
    ctx.command = "printf '[Info] 1600.00 MKey/s\\nFinished\\n'";
    ctx.echoOutput = false;

    auto result = scheduler.runExecution(ctx, executionManager);

    if (!result.success) return 1;
    if (result.jobId != 42) return 2;
    if (result.rangeId != 1001) return 3;
    if (result.exitCode != 0) return 4;

    return 0;
}
CPP

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "SchedulerExecutionTests" not in txt:
    txt += """

add_executable(SchedulerExecutionTests
    core/SchedulerExecutionTests.cpp
)

target_link_libraries(SchedulerExecutionTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerExecutionTests
    COMMAND SchedulerExecutionTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh
