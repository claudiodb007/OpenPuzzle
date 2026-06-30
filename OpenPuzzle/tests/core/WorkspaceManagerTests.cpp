#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>

using namespace openpuzzle;

int main() {
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_workspace_test";

    std::filesystem::remove_all(temp);

    WorkspaceManager manager(temp);

    auto workspace = manager.createJobWorkspace(42);

    if (!std::filesystem::exists(workspace)) return 1;
    if (workspace.filename() != "00000042") return 2;
    if (manager.bitcrackLog(42).filename() != "bitcrack.log") return 3;
    if (manager.stdoutLog(42).filename() != "stdout.log") return 4;
    if (manager.stderrLog(42).filename() != "stderr.log") return 5;
    if (manager.executionFile(42).filename() != "execution.json") return 6;
    if (manager.stateFile(42).filename() != "state.json") return 7;

    std::filesystem::remove_all(temp);

    return 0;
}
