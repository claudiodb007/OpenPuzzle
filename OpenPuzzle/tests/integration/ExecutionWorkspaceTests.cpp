#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>

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

    auto stdoutLog = workspaceManager.stdoutLog(ctx.jobId);

    {
        std::ofstream out(stdoutLog);
        out << "[Info] 1334.62 MKey/s\n";
        out << "Finished\n";
    }

    if (!std::filesystem::exists(stdoutLog)) return 2;

    ExecutionManager manager;
    auto result = manager.run(ctx);

    if (!result.success) return 3;
    if (result.exitCode != 0) return 4;
    if (result.linesRead != 2) return 5;
    if (result.averageSpeed != 1334.62) return 6;

    auto executionFile = workspaceManager.executionFile(ctx.jobId);
    if (!std::filesystem::exists(executionFile)) return 7;

    auto stateFile = workspaceManager.stateFile(ctx.jobId);
    if (!std::filesystem::exists(stateFile)) return 8;

    std::filesystem::remove_all(temp);

    return 0;
}
