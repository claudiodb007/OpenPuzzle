#include "openpuzzle/runtime/DaemonRunner.hpp"

#include "openpuzzle/database/Database.hpp"

#include "openpuzzle/runtime/DaemonStatusCollector.hpp"
#include "openpuzzle/runtime/SchedulerTick.hpp"
#include "openpuzzle/runtime/RuntimeDispatcher.hpp"
#include "openpuzzle/runtime/ExecutionResource.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentFactory.hpp"
#include "openpuzzle/workers/WorkerLifecycle.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace openpuzzle {

DaemonRunner::DaemonRunner(Database& database)
    : database_(database) {
  loadWorkers();
}

void DaemonRunner::loadWorkers() {
  workers_.clear();

  GpuProfileManager profiles(database_);

  for (const auto& record : database_.listWorkers()) {
    WorkerAgentInfo info;

    info.workerId = record.id;
    info.machine = record.machine;
    info.gpuName = record.gpuName;
    info.backend = record.backend;
    info.engine = record.engine;
    info.state =
        WorkerAgent::stateFromString(record.status);
    info.speedMkeys = record.speedMkeys;
    info.temperature = record.temperature;
    info.power = record.power;

    ExecutionResource resource;

    resource.id =
        record.backend + ":" +
        std::to_string(record.id);

    resource.name = record.gpuName;
    resource.engine = record.engine;
    resource.backend = record.backend;
    resource.device = 0;
    resource.available =
        record.status != "offline";

    resource.capability.engine = record.engine;
    resource.capability.backend = record.backend;
    resource.capability.device = resource.device;
    resource.capability.available =
        resource.available;
    resource.capability.benchmarkSpeedMkeys =
        record.speedMkeys;

    auto profile = profiles.chooseBest(
        record.gpuName,
        record.backend,
        record.engine);

    if (profile) {
      resource.capability.blocks =
          profile->blocks;

      resource.capability.threads =
          profile->threads;

      resource.capability.points =
          profile->points;

      if (resource.capability.benchmarkSpeedMkeys <= 0.0) {
        resource.capability.benchmarkSpeedMkeys =
            profile->averageSpeed;
      }
    }

    auto agent =
        WorkerAgentFactory::create(
            resource,
            std::move(info));

    workers_.add(std::move(agent));
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



void DaemonRunner::tick() {
  ++tickCount_;

  WorkerLifecycle lifecycle(
      database_,
      workers_);

  lifecycle.refreshHeartbeats();

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

  auto summary =
      lifecycle.monitorExecutions();

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
