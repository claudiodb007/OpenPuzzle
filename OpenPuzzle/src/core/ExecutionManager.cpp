#include "openpuzzle/core/ExecutionManager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace openpuzzle {

ExecutionSummary ExecutionManager::runCommand(const std::string &command,
                                              bool echoOutput) const {
  ExecutionSummary summary;

  ProcessRunner runner;
  bitcrack::BitCrackOutputParser parser;

  auto result = runner.run(command, [&](const std::string &line) {
    summary.totalLines++;

    if (echoOutput) {
      std::cout << line << "\n";
    }

    auto parsed = parser.parse(line);

    switch (parsed.type) {
    case bitcrack::ParsedLineType::Speed:
      summary.speedEvents++;
      summary.lastSpeedMKeys = parsed.speedMKeys;
      break;
    case bitcrack::ParsedLineType::Error:
      summary.errorEvents++;
      break;
    case bitcrack::ParsedLineType::Found:
      summary.foundEvents++;
      break;
    case bitcrack::ParsedLineType::Finished:
      summary.finishedEvents++;
      break;
    default:
      break;
    }
  });

  summary.started = result.started;
  summary.exitCode = result.exitCode;

  return summary;
}

ExecutionResult ExecutionManager::run(const ExecutionContext &context) const {
  ExecutionResult result;

  std::ofstream stdoutLog;

  if (!context.workspace.empty()) {
    std::filesystem::create_directories(context.workspace);

    stdoutLog.open(std::filesystem::path(context.workspace) / "stdout.log",
                   std::ios::app);

    std::ofstream executionFile(std::filesystem::path(context.workspace) /
                                "execution.json");

    if (executionFile.is_open()) {
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

    std::ofstream stateFile(std::filesystem::path(context.workspace) /
                            "state.json");

    if (stateFile.is_open()) {
      stateFile << "{\n";
      stateFile << "  \"status\": \"RUNNING\",\n";
      stateFile << "  \"exit_code\": -1,\n";
      stateFile << "  \"lines_read\": 0,\n";
      stateFile << "  \"average_speed\": 0,\n";
      stateFile << "  \"keys_checked\": \"0\"\n";
      stateFile << "}\n";
    }
  }

  ProcessRunner runner;
  bitcrack::BitCrackOutputParser parser;

  auto processResult =
      runner.run(context.command, [&](const std::string &line) {
        result.linesRead++;

        if (context.echoOutput) {
          std::cout << line << "\n";
        }

        if (stdoutLog.is_open()) {
          stdoutLog << line << "\n";
          stdoutLog.flush();
        }

        auto parsed = parser.parse(line);

        if (parsed.type == bitcrack::ParsedLineType::Speed) {
          result.averageSpeed = parsed.speedMKeys;

          if (!parsed.totalKeys.empty()) {
            result.keysChecked = parsed.totalKeys;
          }
        }

        if (parsed.type == bitcrack::ParsedLineType::Found) {
          result.keyFound = true;
          result.privateKey = parsed.value;
        }
      });

  result.exitCode = processResult.exitCode;
  result.success = processResult.started && processResult.exitCode == 0;

  if (result.averageSpeed == 0.0 && !context.workspace.empty()) {
    std::ifstream logFile(std::filesystem::path(context.workspace) /
                          "stdout.log");
    std::string line;

    bitcrack::BitCrackOutputParser fallbackParser;

    while (std::getline(logFile, line)) {
      auto parsed = fallbackParser.parse(line);

      if (parsed.type == bitcrack::ParsedLineType::Speed) {
        result.averageSpeed = parsed.speedMKeys;
      }
    }
  }

  if (!context.workspace.empty()) {
    std::ofstream stateFile(std::filesystem::path(context.workspace) /
                            "state.json");

    if (stateFile.is_open()) {
      stateFile << "{\n";
      stateFile << "  \"status\": \""
                << (result.success ? "FINISHED" : "FAILED") << "\",\n";
      stateFile << "  \"exit_code\": " << result.exitCode << ",\n";
      stateFile << "  \"lines_read\": " << result.linesRead << ",\n";
      stateFile << "  \"average_speed\": " << result.averageSpeed << ",\n";
      stateFile << "  \"keys_checked\": \"" << result.keysChecked << "\"\n";
      stateFile << "}\n";
    }
  }

  return result;
}

} // namespace openpuzzle
