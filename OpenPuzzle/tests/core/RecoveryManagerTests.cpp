#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>

using namespace openpuzzle;

int main() {
  auto temp =
      std::filesystem::temp_directory_path() / "openpuzzle_recovery_test";
  std::filesystem::remove_all(temp);

  WorkspaceManager workspace(temp);
  workspace.createJobWorkspace(42);

  {
    std::ofstream state(workspace.stateFile(42));
    state << "{\n";
    state << "  \"status\": \"FINISHED\",\n";
    state << "  \"exit_code\": 0,\n";
    state << "  \"lines_read\": 2,\n";
    state << "  \"average_speed\": 1334.62\n";
    state << "}\n";
  }

  RecoveryManager recovery(workspace);

  if (!recovery.hasStateFile(42))
    return 1;

  auto state = recovery.load(42);

  if (state.jobId != 42)
    return 2;
  if (state.status != RecoveryStatus::Finished)
    return 3;
  if (state.exitCode != 0)
    return 4;
  if (state.linesRead != 2)
    return 5;
  if (state.averageSpeed != 1334.62)
    return 6;

  std::filesystem::remove_all(temp);

  return 0;
}
