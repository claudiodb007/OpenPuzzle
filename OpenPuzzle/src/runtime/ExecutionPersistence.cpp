#include "openpuzzle/runtime/ExecutionPersistence.hpp"

#include <filesystem>
#include <fstream>

namespace openpuzzle {

void ExecutionPersistence::writeExecutionFile(
    const ExecutionContext& context) const {
  if (context.workspace.empty()) {
    return;
  }

  std::ofstream executionFile(std::filesystem::path(context.workspace) /
                              "execution.json");

  if (!executionFile.is_open()) {
    return;
  }

  executionFile << "{\n";
  executionFile << "  \"execution_id\": " << context.executionId << ",\n";
  executionFile << "  \"puzzle_id\": " << context.puzzleId << ",\n";
  executionFile << "  \"job_id\": " << context.jobId << ",\n";
  executionFile << "  \"range_id\": " << context.rangeId << ",\n";
  executionFile << "  \"engine\": \"" << context.engine << "\",\n";
  executionFile << "  \"command\": \"" << context.command << "\",\n";
  executionFile << "  \"workspace\": \"" << context.workspace << "\",\n";
  executionFile << "  \"echo_output\": "
                << (context.echoOutput ? "true" : "false") << "\n";
  executionFile << "}\n";
}

void ExecutionPersistence::writeStateFile(
    const ExecutionContext& context,
    const std::string& status,
    const ExecutionResult& result) const {
  if (context.workspace.empty()) {
    return;
  }

  std::ofstream stateFile(std::filesystem::path(context.workspace) /
                          "state.json");

  if (!stateFile.is_open()) {
    return;
  }

  stateFile << "{\n";
  stateFile << "  \"status\": \"" << status << "\",\n";
  stateFile << "  \"exit_code\": " << result.exitCode << ",\n";
  stateFile << "  \"lines_read\": " << result.linesRead << ",\n";
  stateFile << "  \"average_speed\": " << result.averageSpeed << ",\n";
  stateFile << "  \"keys_checked\": \"" << result.keysChecked << "\"\n";
  stateFile << "}\n";
}

} // namespace openpuzzle
