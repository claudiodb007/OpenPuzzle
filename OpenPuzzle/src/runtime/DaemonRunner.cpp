#include "openpuzzle/runtime/DaemonRunner.hpp"

#include "openpuzzle/database/Database.hpp"

#include "openpuzzle/client/ClientHeartbeatService.hpp"
#include "openpuzzle/client/ClientRegistrationService.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/runtime/DaemonStatusCollector.hpp"
#include "openpuzzle/runtime/RuntimeCoordinator.hpp"
#include "openpuzzle/runtime/ExecutionResource.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentFactory.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace openpuzzle {

DaemonRunner::DaemonRunner(Database& database)
    : DaemonRunner(
          database,
          "https://claudiodb.com",
          60) {}

DaemonRunner::DaemonRunner(
    Database& database,
    std::string serverUrl,
    int syncIntervalSeconds)
    : database_(database),
      serverUrl_(
          std::move(serverUrl)),
      syncInterval_(
          std::chrono::seconds(
              syncIntervalSeconds < 0
                  ? 0
                  : syncIntervalSeconds)) {
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

  registerClient();

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



void DaemonRunner::registerClient() {
  client::ClientRegistrationService service;

  const auto result =
      service.registerWith(
          serverUrl_);

  if (result.success) {
    std::cout
        << "Client registration registered\n"
        << "Client ID......... "
        << result.registration.clientId
        << "\n";

    return;
  }

  /*
   * O daemon deve continuar a trabalhar mesmo
   * quando o servidor está temporariamente
   * indisponível.
   */
  std::cout
      << "Client registration failed\n"
      << "Registration error. "
      << result.error
      << "\n";
}

void DaemonRunner::syncClientExecution() {
  const auto now =
      std::chrono::steady_clock::now();

  if (hasSynced_ &&
      now - lastSyncAt_ <
          syncInterval_) {
    return;
  }

  hasSynced_ = true;
  lastSyncAt_ = now;

  {
    client::ClientHeartbeatService
        heartbeatService;

    const auto heartbeat =
        heartbeatService.send(
            serverUrl_);

    if (heartbeat.success) {
      std::cout
          << "Client heartbeat.. uploaded\n"
          << "Client status..... "
          << heartbeat.heartbeat.status
          << "\n"
          << "Client CPU........ "
          << heartbeat.heartbeat.cpu.name
          << "\n"
          << "Client GPUs....... "
          << heartbeat.heartbeat.gpus.size()
          << "\n";
    } else {
      std::cout
          << "Client heartbeat.. failed\n"
          << "Heartbeat error... "
          << heartbeat.error
          << "\n";
    }
  }

  client::ExecutionSyncService service;

  const auto result =
      service.tick(
          serverUrl_);

  if (!result.hasState) {
    std::cout
        << "Client sync....... idle\n";

    return;
  }

  if (result.running) {
    if (!result.hasProgress) {
      std::cout
          << "Client sync....... "
          << "waiting for progress\n";

      return;
    }

    if (result.progressUploaded) {
      std::cout
          << "Client sync....... "
          << "progress uploaded\n"
          << "Client speed...... "
          << result.progress.speedMKeys
          << " MKey/s\n"
          << "Client keys....... "
          << result.progress.keysChecked
          << "\n";
    } else {
      std::cout
          << "Client sync....... "
          << "progress failed\n"
          << "Client error...... "
          << result.progressError
          << "\n";
    }

    return;
  }

  if (!result.hasExitCode) {
    std::cout
        << "Client sync....... "
        << "waiting for exit code\n";

    return;
  }

  if (result.exitCode != 0) {
    std::cout
        << "Client sync....... "
        << "execution failed\n"
        << "Client exit....... "
        << result.exitCode
        << "\n";

    return;
  }

  if (!result.completionUploaded) {
    std::cout
        << "Client sync....... "
        << "completion failed\n"
        << "Client error...... "
        << result.completionError
        << "\n";

    return;
  }

  if (!result.stateRemoved) {
    std::cout
        << "Client sync....... "
        << "completion uploaded\n"
        << "Client warning.... "
        << result.completionError
        << "\n";

    return;
  }

  std::cout
      << "Client sync....... "
      << "completion uploaded\n";
}

void DaemonRunner::tick() {
  ++tickCount_;

  syncClientExecution();

  DaemonStatusCollector collector(database_);
  auto status = collector.collect();

  std::cout << "Tick.............. "
            << tickCount_ << "\n";

  std::cout << "Reserved jobs..... "
            << status.reservedJobs << "\n";

  std::cout << "Running jobs...... "
            << status.runningJobs << "\n";

  std::cout << "Running executions "
            << status.runningExecutions << "\n";

  std::cout << "Workers........... "
            << status.workers << "\n";

  RuntimeCoordinator coordinator(
      database_,
      workers_,
      schedulingPolicy_,
      engineManager_);

  auto result = coordinator.tick();

  if (result.dispatched) {
    std::cout << "Decision.......... dispatch job "
              << result.decision.jobId
              << " to worker "
              << result.decision.workerId
              << "\n";

    std::cout << "Execution result.. "
              << (result.launchSuccess
                      ? "success"
                      : "failed")
              << "\n";

    std::cout << "Exit code......... "
              << result.exitCode
              << "\n";
  } else {
    std::cout << "Decision.......... idle\n";
  }

  if (result.monitor.finished ||
      result.monitor.failed ||
      result.monitor.cancelled) {
    std::cout
        << "Monitor........... "
        << result.monitor.finished
        << " finished, "
        << result.monitor.failed
        << " failed, "
        << result.monitor.cancelled
        << " cancelled\n";
  }
}

} // namespace openpuzzle
