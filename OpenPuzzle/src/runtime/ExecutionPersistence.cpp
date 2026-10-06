#include "openpuzzle/runtime/ExecutionPersistence.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include "openpuzzle/core/JsonNumber.hpp"
#include "openpuzzle/core/AtomicFile.hpp"

#include <filesystem>
#include <cmath>
#include <locale>
#include <sstream>

namespace openpuzzle {

void ExecutionPersistence::writeExecutionFile(
    const ExecutionContext& context) const {
  if (context.workspace.empty()) {
    return;
  }

  std::ostringstream executionFile;
  executionFile.imbue(std::locale::classic());

  executionFile << "{\n";
  executionFile << "  \"execution_id\": " << context.executionId << ",\n";
  executionFile << "  \"puzzle_id\": " << context.puzzleId << ",\n";
  executionFile << "  \"job_id\": " << context.jobId << ",\n";
  executionFile << "  \"range_id\": " << context.rangeId << ",\n";
  executionFile << "  \"engine\": \"" << escapeJsonString(context.engine) << "\",\n";
  executionFile << "  \"command\": \"" << escapeJsonString(context.command) << "\",\n";
  executionFile << "  \"workspace\": \"" << escapeJsonString(context.workspace) << "\",\n";
  executionFile << "  \"echo_output\": "
                << (context.echoOutput ? "true" : "false") << "\n";
  executionFile << "}\n";
  writePrivateFileAtomically(
      std::filesystem::path(context.workspace) / "execution.json", executionFile.str());
}

void ExecutionPersistence::writeStateFile(
    const ExecutionContext& context,
    const std::string& status,
    const ExecutionResult& result) const {
  if (context.workspace.empty()) {
    return;
  }

  if (!std::isfinite(result.averageSpeed)) {
    return;
  }
  std::ostringstream stateFile;
  stateFile.imbue(std::locale::classic());

  stateFile << "{\n";
  stateFile << "  \"status\": \"" << escapeJsonString(status) << "\",\n";
  stateFile << "  \"exit_code\": " << result.exitCode << ",\n";
  stateFile << "  \"lines_read\": " << result.linesRead << ",\n";
  stateFile << "  \"average_speed\": " << encodeJsonDouble(result.averageSpeed) << ",\n";
  stateFile << "  \"keys_checked\": \"" << escapeJsonString(result.keysChecked) << "\"\n";
  stateFile << "}\n";
  writePrivateFileAtomically(
      std::filesystem::path(context.workspace) / "state.json", stateFile.str());
}

} // namespace openpuzzle
