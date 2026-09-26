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

ClientRuntime::ClientRuntime(
    ClientRuntimeDependencies dependencies)
    : dependencies_(
          std::move(dependencies)) {
  if (!dependencies_.sync ||
      !dependencies_.heartbeat ||
      !dependencies_.stopExecution ||
      !dependencies_.reportSolution ||
      !dependencies_.finalizeAssignment ||
      !dependencies_.finalKeysChecked ||
      !dependencies_.removeState ||
      !dependencies_.hasState ||
      !dependencies_.acquireRuntime ||
      !dependencies_.releaseRuntime ||
      !dependencies_.stopRequested ||
      !dependencies_.safeStopRequested ||
      !dependencies_.clearSafeStop ||
      !dependencies_.prepareSignals ||
      !dependencies_.sleep) {
    throw std::invalid_argument(
        "Incomplete ClientRuntime dependencies");
  }
}

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
                latestProgress(workspace);

        return progress
            ? progress->keysChecked
            : std::string{};
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

bool ClientRuntime::sleepInterruptibly(
    std::chrono::seconds duration) const {
  for (std::chrono::seconds elapsed{0};
       elapsed < duration;
       elapsed += std::chrono::seconds(1)) {
    if (dependencies_.thermalPoll) {
      (void)dependencies_.thermalPoll();
    }

    if (dependencies_.stopRequested()) {
      return false;
    }

    dependencies_.sleep(
        std::chrono::seconds(1));
  }

  return true;
}

int ClientRuntime::runContinuous(
    const std::string &serverUrl,
    const std::function<ClientIterationResult()> &
        executeAssignment) const {
  if (!dependencies_.acquireRuntime()) {
    std::cerr
        << "OpenPuzzle is already running "
        << "or runtime state cannot be created.\n";

    return 1;
  }

  struct RuntimeReleaseGuard {
    const ClientRuntimeDependencies &
        dependencies;

    ~RuntimeReleaseGuard() {
      dependencies.clearSafeStop();
      dependencies.releaseRuntime();
    }
  };

  const RuntimeReleaseGuard releaseGuard{
      dependencies_
  };

  dependencies_.prepareSignals();

  constexpr auto retryInterval =
      std::chrono::seconds(30);

  /*
   * Depois de uma atribuição terminar, o estado
   * local é removido e o heartbeat passa a anunciar
   * o cliente como idle.
   *
   * O servidor deve confirmar essa disponibilidade
   * antes de o runtime pedir a atribuição seguinte.
   */
  bool availabilityHeartbeatRequired = false;

  while (true) {
    /*
     * Sample before requesting work. With critical protection enabled,
     * thermalPoll requests the normal runtime stop and the assignment
     * callback is therefore never entered while a selected GPU is already
     * above the critical threshold.
     */
    const bool thermalStartupBlocked =
        dependencies_.thermalPoll &&
        dependencies_.thermalPoll();

    if (thermalStartupBlocked) {
      std::cerr
          << "\nOpenPuzzle thermal startup protection\n"
          << "-------------------------------------\n"
          << "Startup............. blocked\n"
          << "Assignment......... not requested\n"
          << "Action.............. allow the selected GPU to cool\n";
    }

    if (dependencies_.stopRequested()) {
      std::cout << "openpuzzle stopped.\n";

      return 0;
    }

    if (
        dependencies_.safeStopRequested() &&
        !dependencies_.hasState()) {
      std::cout
          << "Safe stop.......... complete\n"
          << "New assignment..... blocked\n"
          << "openpuzzle stopped.\n";

      return 0;
    }

    if (availabilityHeartbeatRequired) {
      const auto heartbeat =
          dependencies_.heartbeat(
              serverUrl);

      if (!heartbeat.success) {
        std::cerr
            << "Availability heartbeat failed.\n";

        if (!heartbeat.error.empty()) {
          std::cerr
              << "Reason............. "
              << heartbeat.error
              << '\n';
        }

        std::cerr
            << "Retrying........... in 30 seconds\n";

        sleepInterruptibly(
            retryInterval);

        continue;
      }

      availabilityHeartbeatRequired = false;

      std::cout
          << "Client status....... idle\n";
    }

    const auto result =
        executeAssignment();

    /*
     * A execução monitorizada pode ter terminado
     * devido a SIGINT ou SIGTERM.
     */
    if (dependencies_.stopRequested()) {
      return result.exitCode;
    }

    switch (result.status) {
    case ClientIterationStatus::Completed:
      /*
       * A conclusão remove client.state. O próximo
       * heartbeat será portanto recolhido como idle.
       */
      if (dependencies_.safeStopRequested()) {
        std::cout
            << "\nSafe stop.......... complete\n"
            << "Current assignment. completed\n"
            << "New assignment..... blocked\n";

        return 0;
      }

      availabilityHeartbeatRequired = true;

      std::cout
          << "\nPreparing next assignment...\n";

      continue;

    case ClientIterationStatus::Unavailable:
      std::cout
          << "Work............... unavailable\n";

      if (!result.message.empty()) {
        std::cout
            << "Reason............. "
            << result.message
            << '\n';
      }

      std::cout
          << "Retrying........... in 30 seconds\n";

      sleepInterruptibly(retryInterval);

      continue;

    case ClientIterationStatus::Retry:
      std::cerr
          << "Network............ temporarily unavailable\n";

      if (!result.message.empty()) {
        std::cerr
            << "Reason............. "
            << result.message
            << '\n';
      }

      std::cerr
          << "Retrying........... in 30 seconds\n";

      sleepInterruptibly(retryInterval);

      continue;

    case ClientIterationStatus::SolutionFound:
      std::cout
          << "Continuous execution stopped "
          << "after local solution detection.\n";

      return 0;

    case ClientIterationStatus::Failed:
      return result.exitCode == 0
                 ? 1
                 : result.exitCode;
    }
  }
}

int ClientRuntime::run(
    const std::string &serverUrl,
    const std::string &assignmentId,
    const std::string &clientId,
    const std::string &workspace) const {
  dependencies_.prepareSignals();

  constexpr auto solutionReportRetryInterval =
      std::chrono::seconds(30);

  const auto reportDetectedSolution =
      [&] {
        while (true) {
          std::string error;

          if (dependencies_.reportSolution(
                  serverUrl,
                  assignmentId,
                  clientId,
                  error)) {
            std::cout
                << "Solution report.... "
                << "pending review\n";

            return;
          }

          std::cerr
              << "Solution report.... failed\n";

          if (!error.empty()) {
            std::cerr
                << "Reason............. "
                << error
                << '\n';
          }

          std::cerr
              << "Local state........ preserved "
              << "for retry\n";

          if (dependencies_.stopRequested()) {
            std::cerr
                << "Solution retry..... interrupted\n";

            return;
          }

          std::cerr
              << "Retrying........... in 30 seconds\n";

          if (!sleepInterruptibly(
                  solutionReportRetryInterval)) {
            std::cerr
                << "Solution retry..... interrupted\n";

            return;
          }
        }
      };

  const auto exportDetectedSolution =
      [&](const client::ExecutionSyncResult &solution) {
        const auto exported =
            client::SolutionExporter::exportSolution(
                solution.state,
                solution.solutionPath);

        if (!exported.success) {
          std::cerr
              << "Wallet export..... failed\n"
              << "Reason............. "
              << exported.error
              << '\n'
              << "Engine result...... preserved\n";

          return false;
        }

        std::cout
            << "\n"
            << "========================================\n"
            << "PRIVATE KEY FOUND - ACTION REQUIRED\n"
            << "========================================\n"
            << "Wallet file........ "
            << exported.walletPath
            << '\n';

        if (!exported.noticePath.empty()) {
          std::cout
              << "Visible notice..... "
              << exported.noticePath
              << '\n';
        }

        std::cout
            << "Format............. "
            << exported.format
            << "\n"
            << "Permissions........ owner only\n"
            << "Private key........ not displayed or uploaded\n"
            << "Action............. protect the wallet file now\n";

        if (!exported.warning.empty()) {
          std::cerr
              << "Notice warning..... "
              << exported.warning
              << '\n';
        }

        return true;
      };

  constexpr auto syncInterval =
      std::chrono::seconds(60);

  constexpr auto completionPollInterval =
      std::chrono::seconds(2);

  while (true) {
    if (dependencies_.thermalPoll) {
      dependencies_.thermalPoll();
    }

    if (dependencies_.stopRequested()) {
      std::cout << "\nStopping search...\n";

      const auto finalSync =
          dependencies_.sync(serverUrl);

      if (finalSync.solutionFound) {
        std::cout
            << "\n"
            << "SOLUTION FOUND\n"
            << "--------------\n"
            << "Engine result...... "
            << finalSync.solutionPath
            << '\n'
            << "Workspace.......... "
            << workspace
            << '\n'
            << "Local state........ preserved\n"
            << "Stopping search engine...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Warning............ unable to "
              << "confirm search engine termination\n";
        } else {
          std::cout
              << "Search engine....... stopped\n";
        }

        exportDetectedSolution(finalSync);
        reportDetectedSolution();

      return SolutionFoundExitCode;
      }

      if (finalSync.hasProgress &&
          finalSync.progressUploaded) {
        std::cout
            << "Final progress...... uploaded\n";
      } else if (finalSync.hasProgress) {
        std::cerr
            << "Final progress...... failed\n"
            << "Reason............. "
            << finalSync.progressError
            << '\n';
      }

      std::cout << "Stopping search engine...\n";

      if (!dependencies_.stopExecution(
              workspace)) {
        std::cerr
            << "Unable to stop search engine cleanly.\n";

        return 1;
      }

      std::cout
          << "Search engine....... stopped\n";

      const std::string finalKeysChecked =
          dependencies_.finalKeysChecked(
              workspace);

      std::string cancellationError;

      client::AssignmentUploadStatus
          cancellationStatus =
              client::AssignmentUploadStatus::
                  NotAttempted;

      if (
          finalSync.progressStatus ==
          client::AssignmentUploadStatus::
              AssignmentRejected) {
        cancellationStatus =
            client::AssignmentUploadStatus::
                AssignmentRejected;
      } else {
        cancellationStatus =
            dependencies_.finalizeAssignment(
                serverUrl,
                assignmentId,
                clientId,
                -2,
                "cancelled",
                finalKeysChecked,
                cancellationError);
      }

      if (
          cancellationStatus !=
              client::AssignmentUploadStatus::
                  Uploaded &&
          cancellationStatus !=
              client::AssignmentUploadStatus::
                  AssignmentRejected) {
        std::cerr
            << "Cancellation upload failed.\n"
            << "Reason............. "
            << cancellationError
            << '\n'
            << "Local state retained for retry.\n";

        return 1;
      }

      if (!dependencies_.removeState()) {
        std::cerr
            << "Unable to remove local "
            << "execution state.\n";

        return 1;
      }

      std::cout
          << "Assignment......... "
          << (
                 cancellationStatus ==
                         client::
                             AssignmentUploadStatus::
                                 Uploaded
                     ? "cancelled"
                     : "already rejected")
          << '\n'
          << "Goodbye.\n";

      return 0;
    }

    const auto result =
        dependencies_.sync(serverUrl);

    if (!result.hasState) {
      /*
       * A ausência momentânea de client.state não prova
       * que o assignment terminou. Sair daqui faria o ciclo
       * contínuo pedir novamente o mesmo assignment e lançar
       * outro engine sobre o mesmo workspace.
       */
      std::cerr
          << "Local state........ temporarily unavailable\n"
          << "Monitoring.......... preserved\n";

      dependencies_.sleep(
          completionPollInterval);

      continue;
    }

    if (result.solutionFound) {
      std::cout
          << "\n"
          << "SOLUTION FOUND\n"
          << "--------------\n"
          << "Engine result...... "
          << result.solutionPath
          << '\n'
          << "Workspace.......... "
          << workspace
          << '\n'
          << "Local state........ preserved\n";

      if (result.running) {
        std::cout
            << "Stopping search engine...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Warning............ unable to "
              << "confirm search engine termination\n";
        } else {
          std::cout
              << "Search engine....... stopped\n";
        }
      }

      exportDetectedSolution(result);
      reportDetectedSolution();

      return SolutionFoundExitCode;
    }

    if (result.running) {
      if (result.hasProgress) {
        std::cout
            << "Speed.............. "
            << result.progress.speedMKeys
            << " MKey/s\n"
            << "Keys checked....... "
            << result.progress.keysChecked
            << '\n';

        if (!result.progressUploaded) {
          std::cerr
              << "Progress upload.... failed\n"
              << "Reason............. "
              << result.progressError
              << '\n';
        }
      } else {
        std::cout
            << "Progress........... "
            << "waiting for engine output\n";
      }

      if (
          result.progressStatus ==
          client::AssignmentUploadStatus::
              AssignmentRejected) {
        if (
            result.progressReason ==
            "puzzle_verification_pending") {
          std::cerr
              << "Puzzle............. paused for solution review\n"
              << "Assignment......... stopped temporarily\n";
        } else if (
            result.progressReason ==
            "puzzle_solved") {
          std::cerr
              << "Puzzle............. solved\n"
              << "Assignment......... stopped permanently\n";
        } else {
          std::cerr
              << "Assignment......... rejected by server\n";
        }

        std::cerr
            << "Stopping search engine...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Unable to stop search engine cleanly.\n";

          return 1;
        }

        if (!dependencies_.removeState()) {
          std::cerr
              << "Unable to remove rejected "
              << "execution state.\n";

          return 1;
        }

        std::cout
            << "Search engine....... stopped\n"
            << "Local state........ removed\n";

        return 0;
      }

      if (
          result.progressStatus ==
          client::AssignmentUploadStatus::
              PermanentFailure) {
        std::cerr
            << "Progress error..... permanent\n"
            << "Stopping search engine...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Unable to stop search engine cleanly.\n";
        }

        std::cerr
            << "Local state retained for diagnosis.\n";

        return 1;
      }

      const auto heartbeat =
          dependencies_.heartbeat(serverUrl);

      if (!heartbeat.success) {
        std::cerr
            << "Heartbeat.......... failed\n"
            << "Reason............. "
            << heartbeat.error
            << '\n';
      }

      sleepInterruptibly(syncInterval);

      continue;
    }

    if (!result.hasExitCode) {
      dependencies_.sleep(
          completionPollInterval);

      continue;
    }

    std::cout
        << "Exit code.......... "
        << result.exitCode
        << '\n';

    if (
        result.completionStatus ==
        client::AssignmentUploadStatus::
            AssignmentRejected) {
      std::cerr
          << "Completion......... rejected by server\n";

      if (!result.stateRemoved) {
        std::cerr
            << "State cleanup...... failed\n"
            << "Reason............. "
            << result.completionError
            << '\n';

        return 1;
      }

      std::cout
          << "Local state........ removed\n"
          << "Requesting new work after "
          << "server rejection.\n";

      return 0;
    }

    if (
        result.completionStatus ==
        client::AssignmentUploadStatus::
            PermanentFailure) {
      std::cerr
          << "Completion error... permanent\n"
          << "Reason............. "
          << result.completionError
          << '\n'
          << "Local state retained for diagnosis.\n";

      return 1;
    }

    if (result.exitCode != 0) {
      std::cerr
          << "Assignment......... failed\n";

      /*
       * ExecutionSyncService já pode ter enviado
       * e limpo o estado da execução falhada.
       */
      if (result.completionUploaded) {
        if (!result.stateRemoved) {
          std::cerr
              << "Failure upload..... uploaded\n"
              << "State cleanup...... failed\n"
              << "Reason............. "
              << result.completionError
              << '\n';

          return 1;
        }

        std::cerr
            << "Failure upload..... uploaded\n"
            << "Local state........ removed\n";

        return 1;
      }

      std::string failureError;

      const auto failureStatus =
          dependencies_.finalizeAssignment(
              serverUrl,
              assignmentId,
              clientId,
              result.exitCode,
              "failed",
              dependencies_.finalKeysChecked(
                  workspace),
              failureError);

      if (
          failureStatus !=
              client::AssignmentUploadStatus::
                  Uploaded &&
          failureStatus !=
              client::AssignmentUploadStatus::
                  AssignmentRejected) {
        std::cerr
            << "Failure upload..... failed\n"
            << "Reason............. "
            << failureError
            << '\n'
            << "Local state retained for retry.\n";

        return 1;
      }

      if (!dependencies_.removeState()) {
        std::cerr
            << "Failure upload..... uploaded\n"
            << "State cleanup...... failed\n";

        return 1;
      }

      std::cerr
          << "Failure upload..... "
          << (
                 failureStatus ==
                         client::
                             AssignmentUploadStatus::
                                 Uploaded
                     ? "uploaded"
                     : "rejected by server")
          << '\n'
          << "Local state........ removed\n";

      return 1;
    }

    if (!result.completionUploaded) {
      std::cerr
          << "Completion upload.. failed\n"
          << "Reason............. "
          << result.completionError
          << '\n';

      sleepInterruptibly(syncInterval);

      continue;
    }

    if (!result.stateRemoved) {
      std::cerr
          << "State cleanup...... failed\n"
          << "Reason............. "
          << result.completionError
          << '\n';

      return 1;
    }

    if (result.calibrationAttempted) {
      if (result.calibrationUpdated) {
        std::cout
            << "Profile calibration updated\n"
            << "Planning speed..... "
            << result.calibratedPlanningSpeed
            << " MKey/s\n"
            << "Accepted samples... "
            << result.calibrationSamples
            << '\n';
      } else {
        std::cout
            << "Profile calibration skipped\n";
        if (!result.calibrationError.empty()) {
          std::cout
              << "Reason............. "
              << result.calibrationError
              << '\n';
        }
      }
    }

    std::cout
        << "Completion......... uploaded\n"
        << "Assignment complete.\n";

    return 0;
  }
}

} // namespace openpuzzle
