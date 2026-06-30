#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>

using namespace openpuzzle;

int main() {
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_execution_workspace_test";
    std::filesystem::remove_all(temp);

    WorkspaceManager workspaceManager(temp);

    ExecutionContext ctx;
    ctx.executionId = 1;
    ctx.puzzleId = 71;
    ctx.jobId = 42;
    ctx.rangeId = 1001;
    ctx.engine = "BitCrack";
    ctx.workspace = workspaceManager.createJobWorkspace(ctx.jobId).string();
    ctx.command = "printf '[Info] 1334.62 MKey/s\\nFinished\\n'";
    ctx.echoOutput = false;

    if (!std::filesystem::exists(ctx.workspace)) return 1;

    ExecutionManager manager;
    auto result = manager.run(ctx);

    if (!result.success) return 2;
    if (result.exitCode != 0) return 3;
    if (result.linesRead != 2) return 4;
    if (result.averageSpeed != 1334.62) return 5;

    std::filesystem::remove_all(temp);

    return 0;
}
