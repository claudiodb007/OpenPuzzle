#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"

using namespace openpuzzle;

int main() {
    ExecutionContext ctx;

    ctx.executionId = 1;
    ctx.puzzleId = 71;
    ctx.jobId = 42;
    ctx.rangeId = 1001;
    ctx.engine = "BitCrack";
    ctx.workspace = "workspace";
    ctx.command = "printf '[Info] 1334.62 MKey/s\\nFinished\\n'";
    ctx.echoOutput = false;

    ExecutionManager manager;
    auto result = manager.run(ctx);

    if (!result.success) return 1;
    if (result.exitCode != 0) return 2;
    if (result.linesRead != 2) return 3;
    if (result.averageSpeed != 1334.62) return 4;
    if (result.keyFound) return 5;

    return 0;
}
