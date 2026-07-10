#include "openpuzzle/runtime/DaemonRunner.hpp"

#include "openpuzzle/database/Database.hpp"

#include "openpuzzle/runtime/DaemonStatusCollector.hpp"
#include "openpuzzle/runtime/SchedulerTick.hpp"
#include "openpuzzle/runtime/RuntimeDispatcher.hpp"
#include "openpuzzle/runtime/ExecutionProcessMonitor.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/services/HeartbeatService.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace openpuzzle {

DaemonRunner::DaemonRunner(Database& database)
    : database_(database) {
  loadWorkers();
}

void DaemonRunner::loadWorkers() {
  workers_.clear();

  for (const auto& record : database_.listWorkers()) {
    WorkerAgentInfo info;

    info.workerId = record.id;
    info.machine = record.machine;
    info.gpuName = record.gpuName;
    info.backend = record.backend;
    info.engine = record.engine;
    info.state = WorkerAgent::stateFromString(record.status);
    info.speedMkeys = record.speedMkeys;
    info.temperature = record.temperature;
    info.power = record.power;

    WorkerEngineCapability capability;
    capability.engine = record.engine;
    capability.backend = record.backend;
    capability.device = 0;
    capability.benchmarkSpeedMkeys =
        record.speedMkeys;

    GpuProfileManager profiles(database_);

    auto profile = profiles.chooseBest(
        record.gpuName,
        record.backend,
        record.engine);

    if (profile) {
      capability.blocks = profile->blocks;
      capability.threads = profile->threads;
      capability.points = profile->points;

      if (capability.benchmarkSpeedMkeys <= 0.0) {
        capability.benchmarkSpeedMkeys =
            profile->averageSpeed;
      }
    }

    info.capabilities.push_back(capability);

    workers_.add(WorkerAgent(info));
  }
}

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

void DaemonRunner::heartbeatWorkers() {
  HeartbeatService heartbeat(database_);

  for (const auto* worker : workers_.all()) {
    if (!worker || worker->offline()) {
      continue;
    }

    heartbeat.update(
        worker->info().workerId,
        WorkerAgent::stateToString(worker->state()),
        worker->info().speedMkeys,
        worker->info().temperature,
        worker->info().power);
  }

  heartbeat.expireStale(30);
}

void DaemonRunner::synchronizeWorkers() {
  for (auto* worker : workers_.all()) {
    if (!worker || !worker->hasExecution()) {
      continue;
    }

    const auto& handle = worker->currentExecution();

    if (!handle) {
      continue;
    }

    auto execution =
        database_.getExecution(handle->executionId);

    if (!execution) {
      continue;
    }

    if (execution->status == ExecutionRecordStatus::Running) {
      continue;
    }

    worker->completeExecution();

    database_.updateWorkerStatus(
        worker->info().workerId,
        WorkerAgent::stateToString(worker->state()));
  }
}

void DaemonRunner::tick() {
  ++tickCount_;

  heartbeatWorkers();

  DaemonStatusCollector collector(database_);
  auto status = collector.collect();

  std::cout << "Tick.............. " << tickCount_ << "\n";
  std::cout << "Reserved jobs..... " << status.reservedJobs << "\n";
  std::cout << "Running jobs...... " << status.runningJobs << "\n";
  std::cout << "Running executions " << status.runningExecutions << "\n";
  std::cout << "Workers........... " << status.workers << "\n";

  SchedulerTick scheduler(database_, schedulingPolicy_);
  auto decision = scheduler.execute();

  RuntimeDispatcher dispatcher(database_, workers_);

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

  synchronizeWorkers();

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
