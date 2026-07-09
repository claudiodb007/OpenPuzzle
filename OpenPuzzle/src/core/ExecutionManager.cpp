#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ProcessRunnerFactory.hpp"
#include "openpuzzle/adapters/bitcrack/BitCrackProgressParser.hpp"
#include "openpuzzle/runtime/Execution.hpp"
#include "openpuzzle/runtime/ExecutionMonitor.hpp"
#include "openpuzzle/runtime/ExecutionPersistence.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace openpuzzle {


ExecutionSummary ExecutionManager::runCommand(const std::string &command,
                                              bool echoOutput) const {
  ExecutionSummary summary;

  auto runner = ProcessRunnerFactory::create();
  bitcrack::BitCrackOutputParser parser;

  auto result = runner->run(command, [&](const std::string &line) {
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

ExecutionResult ExecutionManager::run(const ExecutionContext &context,
                                      int maxSeconds, int maxSamples) const {
  (void)maxSeconds;
  ExecutionResult result;
  result.exitCode = -1;
  result.keysChecked = "0";

  std::ofstream stdoutLog;
  ExecutionPersistence persistence;

  if (!context.workspace.empty()) {
    std::filesystem::create_directories(context.workspace);

    stdoutLog.open(std::filesystem::path(context.workspace) / "stdout.log",
                   std::ios::app);

    persistence.writeExecutionFile(context);
    persistence.writeStateFile(context, "RUNNING", result);
  }

  ExecutionState executionState;
  executionState.executionId = context.executionId;
  executionState.puzzleId = context.puzzleId;
  executionState.jobId = context.jobId;
  executionState.rangeId = context.rangeId;
  executionState.engine = context.engine;
  executionState.workspace = context.workspace;
  executionState.command = context.command;
  executionState.status = RuntimeExecutionStatus::Running;

  Execution execution(executionState);

  auto runner = ProcessRunnerFactory::create();
  ExecutionMonitor monitor(
      std::make_unique<bitcrack::BitCrackProgressParser>());

  monitor.setCallback([&](const ExecutionProgress &progress) {
    if (progress.speedMKeys > 0.0) {
      result.averageSpeed = progress.speedMKeys;

      if (progress.speedMKeys >= 100.0) {
        result.speedSamples.push_back(progress.speedMKeys);
      }
    }

    if (!progress.keysChecked.empty()) {
      result.keysChecked = progress.keysChecked;
    }

    if (progress.keyFound) {
      result.keyFound = true;
      result.privateKey = progress.privateKey;
    }

    persistence.writeStateFile(context, "RUNNING", result);

    if (context.onProgress) {
      context.onProgress(result);
    }
  });

  auto processResult = runner->run(
      context.command,
      [&](const std::string &line) {
        result.linesRead++;

        if (context.echoOutput) {
          std::cout << line << "\n";
        }

        if (stdoutLog.is_open()) {
          stdoutLog << line << "\n";
          stdoutLog.flush();
        }

        monitor.processLine(execution, line);
      },
      maxSeconds,
      [&]() {
        return maxSamples > 0 &&
               static_cast<int>(result.speedSamples.size()) >= maxSamples;
      });

  result.exitCode = processResult.exitCode;
  result.success = processResult.started && processResult.exitCode == 0;

  persistence.writeStateFile(context, result.success ? "FINISHED" : "FAILED", result);

  return result;
}

} // namespace openpuzzle
