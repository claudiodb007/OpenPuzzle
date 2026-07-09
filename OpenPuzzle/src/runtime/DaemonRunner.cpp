#include "openpuzzle/runtime/DaemonRunner.hpp"

#include "openpuzzle/runtime/DaemonStatusCollector.hpp"
#include "openpuzzle/runtime/SchedulerTick.hpp"
#include "openpuzzle/runtime/RuntimeDispatcher.hpp"
#include "openpuzzle/runtime/ExecutionProcessMonitor.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace openpuzzle {

DaemonRunner::DaemonRunner(Database& database)
    : database_(database) {}

int DaemonRunner::run(int ticks) {
  running_ = true;
  tickCount_ = 0;

  std::cout << "OpenPuzzle Daemon\n";
  std::cout << "-----------------\n";
  std::cout << "Status............ starting\n";

  while (running_ && tickCount_ < ticks) {
    tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::cout << "Status............ stopped\n";

  return 0;
}

void DaemonRunner::stop() {
  running_ = false;
}

void DaemonRunner::tick() {
  ++tickCount_;

  DaemonStatusCollector collector(database_);
  auto status = collector.collect();

  std::cout << "Tick.............. " << tickCount_ << "\n";
  std::cout << "Reserved jobs..... " << status.reservedJobs << "\n";
  std::cout << "Running jobs...... " << status.runningJobs << "\n";
  std::cout << "Running executions " << status.runningExecutions << "\n";
  std::cout << "Workers........... " << status.workers << "\n";

  SchedulerTick scheduler(database_);
  auto decision = scheduler.execute();

  RuntimeDispatcher dispatcher(database_);

  if (dispatcher.dispatch(decision)) {
    std::cout << "Decision.......... dispatch job "
              << decision.jobId
              << " to worker "
              << decision.workerId
              << "\n";

    auto result = dispatcher.dispatchAndLaunch(decision);

    std::cout << "Execution result.. "
              << (result.success ? "success" : "failed")
              << "\n";
    std::cout << "Exit code......... " << result.exitCode << "\n";
  } else {
    std::cout << "Decision.......... idle\n";
  }

  ExecutionProcessMonitor monitor(database_);

  auto summary = monitor.poll();

  if (summary.finished ||
      summary.failed ||
      summary.cancelled) {

    std::cout
        << "Monitor........... "
        << summary.finished
        << " finished, "
        << summary.failed
        << " failed, "
        << summary.cancelled
        << " cancelled\n";
  }
}

} // namespace openpuzzle
