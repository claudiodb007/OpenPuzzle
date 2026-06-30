#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace openpuzzle;

static std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

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

    auto executionContent = readFile(executionFile);
    if (executionContent.find("\"success\": true") == std::string::npos) return 9;
    if (executionContent.find("\"average_speed\": 1334.62") == std::string::npos) return 10;

    auto stateContent = readFile(stateFile);
    if (stateContent.find("\"status\": \"FINISHED\"") == std::string::npos) return 11;
    if (stateContent.find("\"average_speed\": 1334.62") == std::string::npos) return 12;

    std::filesystem::remove_all(temp);

    return 0;
}
