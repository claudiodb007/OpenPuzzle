#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionSession.hpp"
#include "openpuzzle/core/ProcessRunner.hpp"

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

} // namespace openpuzzle
