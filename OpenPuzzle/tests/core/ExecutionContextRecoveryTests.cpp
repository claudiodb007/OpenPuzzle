#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>

using namespace openpuzzle;

int main() {
  auto temp =
      std::filesystem::temp_directory_path() / "openpuzzle_context_recovery";

  std::filesystem::remove_all(temp);

  WorkspaceManager workspace(temp);
  workspace.createJobWorkspace(42);

  auto workspacePath = workspace.jobWorkspace(42).string();

  {
    std::ofstream execution(workspace.executionFile(42));
    execution << "{\n";
    execution << "  \"execution_id\": 7,\n";
    execution << "  \"puzzle_id\": 71,\n";
    execution << "  \"job_id\": 42,\n";
    execution << "  \"range_id\": 1001,\n";
    execution << "  \"engine\": \"BitCrack\",\n";
    execution << "  \"command\": \"printf test\",\n";
    execution << "  \"workspace\": \"" << workspacePath << "\",\n";
    execution << "  \"echo_output\": false\n";
    execution << "}\n";
  }

  RecoveryManager recovery(workspace);
  auto ctx = recovery.buildExecutionContext(42);

  if (ctx.executionId != 7)
    return 1;
  if (ctx.puzzleId != 71)
    return 2;
  if (ctx.jobId != 42)
    return 3;
  if (ctx.rangeId != 1001)
    return 4;
  if (ctx.engine != "BitCrack")
    return 5;
  if (ctx.command != "printf test")
    return 6;
  if (ctx.workspace.find("00000042") == std::string::npos)
    return 7;
  if (ctx.echoOutput != false)
    return 8;

  std::filesystem::remove_all(temp);

  return 0;
}
