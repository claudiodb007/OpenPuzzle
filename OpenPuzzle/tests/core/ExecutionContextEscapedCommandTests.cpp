#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>

using namespace openpuzzle;

int main() {
  auto temp = std::filesystem::temp_directory_path() /
              "openpuzzle_escaped_command_recovery";

  std::filesystem::remove_all(temp);

  WorkspaceManager workspace(temp);
  workspace.createJobWorkspace(42);

  {
    std::ofstream execution(workspace.executionFile(42));
    execution << "{\n";
    execution << "  \"execution_id\": 7,\n";
    execution << "  \"puzzle_id\": 71,\n";
    execution << "  \"job_id\": 42,\n";
    execution << "  \"range_id\": 1001,\n";
    execution << "  \"engine\": \"BitCrack\",\n";
    execution << "  \"command\": \"printf \\\"hello\\\"\",\n";
    execution << "  \"workspace\": \"" << workspace.jobWorkspace(42).string()
              << "\",\n";
    execution << "  \"echo_output\": true\n";
    execution << "}\n";
  }

  RecoveryManager recovery(workspace);
  auto ctx = recovery.buildExecutionContext(42);

  if (ctx.command != "printf \"hello\"")
    return 1;

  std::filesystem::remove_all(temp);

  return 0;
}
