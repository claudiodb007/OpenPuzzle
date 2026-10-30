#include "openpuzzle/runtime/ClientRuntime.hpp"

#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/SolutionExporter.hpp"
#include "openpuzzle/core/SignalHandler.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace openpuzzle {

ClientRuntime::ClientRuntime()
    : ClientRuntime(std::nullopt) {}

ClientRuntime::ClientRuntime(
    std::optional<std::vector<std::string>>
        thermalDeviceScope)
    : dependencies_(
          productionDependencies(
              std::move(thermalDeviceScope))) {}

ClientRuntimeDependencies
ClientRuntime::productionDependencies(
    std::optional<std::vector<std::string>>
        thermalDeviceScope) {
  ClientRuntimeDependencies dependencies;

  const auto thermalObserver =
      std::make_shared<RuntimeThermalObserver>(
          ConfigurationManager::load().gpu.thermal,
          std::move(thermalDeviceScope));

  dependencies.sync =
      [](const std::string &serverUrl) {
        client::ExecutionSyncService service;

        return service.tick(serverUrl);
      };

  dependencies.heartbeat =
      [](const std::string &serverUrl) {
        client::ClientHeartbeatService service;

        return service.send(serverUrl);
      };

  dependencies.stopExecution =
      [](const std::string &workspace) {
        ExecutionStopper stopper;

        return stopper.stop(workspace);
      };

  dependencies.reportSolution =
      [](const std::string &serverUrl,
         const std::string &assignmentId,
         const std::string &clientId,
         std::string &error) {
        client::HttpRangeClient httpClient(
            serverUrl);

        const bool uploaded =
            httpClient.reportSolution(
                assignmentId,
                clientId);

        error =
            httpClient.lastError();

        return uploaded;
      };

  dependencies.finalizeAssignment =
      [](const std::string &serverUrl,
         const std::string &assignmentId,
         const std::string &clientId,
         int exitCode,
         const std::string &status,
         const std::string &keysChecked,
         std::string &error) {
        client::HttpRangeClient httpClient(
            serverUrl);

        if (httpClient.complete(
                assignmentId,
                clientId,
                exitCode,
                status,
                keysChecked)) {
          return
              client::AssignmentUploadStatus::
                  Uploaded;
        }

        error =
            httpClient.lastError();

        return
            client::ExecutionSyncService::
                classifyCompletionError(
                    httpClient.lastErrorCode());
      };

  dependencies.finalKeysChecked =
      [](const std::string &workspace) {
        const auto progress =
            client::ExecutionSyncService::
                latestProgress(workspace,
                    client::ClientStateStore::load().value_or(client::ClientExecutionState{}).engine);

        return progress
            ? progress->keysChecked
            : std::string{};
      };

  dependencies.prepareCancellation = [](const std::string& workspace) {
    auto state=client::ClientStateStore::load();
    return state && state->workspace==workspace && client::ExecutionSyncService::requestCancellation(*state);
  };

  dependencies.removeState =
      [] {
        return client::ClientStateStore::remove();
      };

  dependencies.hasState =
      [] {
        return
            client::ClientStateStore::load()
                .has_value();
      };

  dependencies.acquireRuntime =
      [] {
        return ClientRuntimeControl::acquire();
      };

  dependencies.releaseRuntime =
      [] {
        ClientRuntimeControl::release();
      };

  dependencies.stopRequested =
      [] {
        return SignalHandler::stopRequested();
      };

  dependencies.safeStopRequested =
      [] {
        return
            ClientRuntimeControl::
                safeStopRequested();
      };

  dependencies.clearSafeStop =
      [] {
        return
            ClientRuntimeControl::
                clearSafeStop();
      };

  dependencies.prepareSignals =
      [] {
        SignalHandler::reset();
        SignalHandler::install();
      };

  dependencies.sleep =
      [](std::chrono::seconds duration) {
        std::this_thread::sleep_for(duration);
      };

  dependencies.wakeRequested = [notifiedPid=0]() mutable {
    auto state=client::ClientStateStore::load();
    if(!state || state->engine!="Vanity") return false;
    if(state->pid==notifiedPid) return false;
    auto result=client::ExecutionSyncService().inspect();
    if(result.hasState && !result.running && !result.identityUnavailable) {notifiedPid=state->pid;return true;}
    return false;
  };

  dependencies.thermalPoll =
      [thermalObserver, protectionRequested = false]() mutable {
        for (const auto &event : thermalObserver->poll()) {
          RuntimeThermalObserver::print(event, std::cerr);

          if (
              event.kind == RuntimeThermalEventKind::Critical &&
              thermalObserver->protectionEnabled() &&
              !protectionRequested) {
            protectionRequested = true;
            std::cerr
                << "\nOpenPuzzle thermal protection\n"
                << "-----------------------------\n"
                << "Trigger............. critical threshold\n"
                << "Action.............. orderly stop requested\n"
                << "Runtime state....... synchronization preserved\n";
            SignalHandler::requestStop();
          }
        }

        return protectionRequested;
      };

  return dependencies;
}

} // namespace openpuzzle
