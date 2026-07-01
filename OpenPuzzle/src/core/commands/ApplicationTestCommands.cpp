#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionSession.hpp"
#include "openpuzzle/core/ProcessRunner.hpp"
#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"
#include <filesystem>
#include <fstream>

#include <iostream>
#include <string>

namespace openpuzzle {

int Application::cmdProcessTest(const std::vector<std::string> &args) {
  std::string command = getArg(args, "--command", "printf 'line1\\nline2\\n'");

  ProcessRunner runner;

  auto result = runner.run(command, [](const std::string &line) {
    std::cout << "OUT.................. " << line << "\n";
  });

  std::cout << "Started.............. " << (result.started ? "yes" : "no")
            << "\n";
  std::cout << "Exit code............ " << result.exitCode << "\n";

  return result.exitCode == 0 ? 0 : 1;
}

int Application::cmdExecutionTest(const std::vector<std::string> &args) {
  std::string command =
      getArg(args, "--command",
             "printf '[Info] Starting at: 400000000000000000\\n[Info] 1334.62 "
             "MKey/s\\nFinished\\n'");

  ExecutionManager manager;
  auto summary = manager.runCommand(command, true);

  std::cout << "Started.............. " << (summary.started ? "yes" : "no")
            << "\n";
  std::cout << "Exit code............ " << summary.exitCode << "\n";
  std::cout << "Lines................ " << summary.totalLines << "\n";
  std::cout << "Speed events......... " << summary.speedEvents << "\n";
  std::cout << "Last speed........... " << summary.lastSpeedMKeys
            << " MKey/s\n";
  std::cout << "Error events......... " << summary.errorEvents << "\n";
  std::cout << "Found events......... " << summary.foundEvents << "\n";
  std::cout << "Finished events...... " << summary.finishedEvents << "\n";

  return summary.started && summary.exitCode == 0 ? 0 : 1;
}

int Application::cmdSessionTest(const std::vector<std::string> &args) {
  (void)args;

  ExecutionSession session;
  session.executionId = 1;
  session.jobId = 42;
  session.puzzleId = 71;
  session.rangeId = 1001;
  session.engine = "BitCrack";
  session.workspace = "~/.local/share/OpenPuzzle/jobs/000042";
  session.command = "cuBitCrack ...";
  session.status = ExecutionSessionStatus::Created;

  std::cout << "Execution ID......... " << session.executionId << "\n";
  std::cout << "Job ID............... " << session.jobId << "\n";
  std::cout << "Puzzle ID............ " << session.puzzleId << "\n";
  std::cout << "Range ID............. " << session.rangeId << "\n";
  std::cout << "Engine............... " << session.engine << "\n";
  std::cout << "Workspace............ " << session.workspace << "\n";
  std::cout << "Status............... "
            << ExecutionSession::statusToString(session.status) << "\n";

  return 0;
}

int Application::cmdEventTest(const std::vector<std::string> &args) {
  (void)args;

  EventBus bus;
  int received = 0;

  bus.subscribe([&](const Event &event) {
    received++;
    std::cout << "EVENT................ "
              << EventBus::eventTypeToString(event.type) << "\n";

    if (!event.message.empty()) {
      std::cout << "Message.............. " << event.message << "\n";
    }

    if (!event.value.empty()) {
      std::cout << "Value................ " << event.value << "\n";
    }

    if (event.numericValue != 0.0) {
      std::cout << "Numeric.............. " << event.numericValue << "\n";
    }
  });

  bus.publish(
      Event{EventType::ExecutionStarted, 1, 42, "Execution started", "", 0.0});
  bus.publish(
      Event{EventType::Speed, 1, 42, "Speed sample", "MKey/s", 1334.62});
  bus.publish(Event{EventType::ExecutionFinished, 1, 42, "Execution finished",
                    "", 0.0});

  std::cout << "Events received...... " << received << "\n";

  return received == 3 ? 0 : 1;
}

int Application::cmdResumeTest(const std::vector<std::string> &args) {
  (void)args;

  auto base = std::filesystem::temp_directory_path() / "openpuzzle_resume_test";

  WorkspaceManager workspace(base);
  workspace.createJobWorkspace(42);

  {
    std::ofstream out(workspace.stateFile(42));
    out << "{\n";
    out << "  \"status\": \"FINISHED\",\n";
    out << "  \"exit_code\": 0,\n";
    out << "  \"lines_read\": 2,\n";
    out << "  \"average_speed\": 1334.62\n";
    out << "}\n";
  }

  RecoveryManager recovery(workspace);
  auto state = recovery.load(42);

  std::cout << "Job............. " << state.jobId << "\n";
  std::cout << "Status.......... "
            << (state.status == RecoveryStatus::Finished  ? "FINISHED"
                : state.status == RecoveryStatus::Running ? "RUNNING"
                : state.status == RecoveryStatus::Failed  ? "FAILED"
                                                          : "UNKNOWN")
            << "\n";
  std::cout << "Exit code....... " << state.exitCode << "\n";
  std::cout << "Lines........... " << state.linesRead << "\n";
  std::cout << "Speed........... " << state.averageSpeed << " MKey/s\n";

  std::filesystem::remove_all(base);

  return state.status == RecoveryStatus::Finished ? 0 : 1;
}

int Application::cmdResume(const std::vector<std::string> &args) {
  int jobId = getIntArg(args, "--job", 42);

  std::filesystem::path base;

  auto customBase = getArg(args, "--base", "");
  if (!customBase.empty()) {
    base = customBase;
  } else {
    auto home = std::getenv("HOME");
    if (!home) {
      std::cerr << "Error: HOME not set\n";
      return 1;
    }
    base = std::filesystem::path(home) / ".local" / "share" / "OpenPuzzle";
  }

  WorkspaceManager workspace(base);
  RecoveryManager recovery(workspace);

  if (!recovery.hasStateFile(jobId)) {
    std::cerr << "No recovery state found for job " << jobId << "\n";
    return 1;
  }

  auto state = recovery.load(jobId);

  std::cout << "====================================\n";
  std::cout << "        OpenPuzzle Recovery\n";
  std::cout << "====================================\n\n";

  std::cout << "Job............. " << state.jobId << "\n";
  std::cout << "Status.......... "
            << (state.status == RecoveryStatus::Finished  ? "FINISHED"
                : state.status == RecoveryStatus::Running ? "RUNNING"
                : state.status == RecoveryStatus::Failed  ? "FAILED"
                                                          : "UNKNOWN")
            << "\n";
  std::cout << "Exit code....... " << state.exitCode << "\n";
  std::cout << "Lines........... " << state.linesRead << "\n";
  std::cout << "Speed........... " << state.averageSpeed << " MKey/s\n";
  std::cout << "Workspace....... " << workspace.jobWorkspace(jobId) << "\n";

  if (hasArg(args, "--run")) {
    std::cout << "\n";
    std::cout << "Rebuilding execution context...\n";

    auto ctx = recovery.buildExecutionContext(jobId);

    ExecutionManager manager;
    auto result = manager.run(ctx);

    std::cout << "\nExecution finished\n";
    std::cout << "Exit code....... " << result.exitCode << "\n";
    std::cout << "Lines........... " << result.linesRead << "\n";
    std::cout << "Average speed... " << result.averageSpeed << " MKey/s\n";
  }

  return 0;
}

} // namespace openpuzzle
