#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>

using namespace openpuzzle;

int main() {
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_recovery_test";
    std::filesystem::remove_all(temp);

    WorkspaceManager workspace(temp);
    workspace.createJobWorkspace(42);

    {
        std::ofstream state(workspace.stateFile(42));
        state << "{\n";
        state << "  \"status\": \"FINISHED\"\n";
        state << "}\n";
    }

    RecoveryManager recovery(workspace);

    if (!recovery.hasStateFile(42)) return 1;

    auto content = recovery.readState(42);

    if (content.find("\"status\": \"FINISHED\"") == std::string::npos) return 2;

    std::filesystem::remove_all(temp);

    return 0;
}
