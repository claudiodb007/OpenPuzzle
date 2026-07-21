#include "openpuzzle/runtime/ClientRuntime.hpp"

#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/client/SolutionExporter.hpp"
#include "openpuzzle/core/SignalHandler.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace openpuzzle {

ClientRuntime::ClientRuntime()
    : dependencies_(
          productionDependencies()) {}

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
      !dependencies_.acquireRuntime ||
      !dependencies_.releaseRuntime ||
      !dependencies_.stopRequested ||
      !dependencies_.prepareSignals ||
      !dependencies_.sleep) {
    throw std::invalid_argument(
        "Incomplete ClientRuntime dependencies");
  }
}

ClientRuntimeDependencies
ClientRuntime::productionDependencies() {
  ClientRuntimeDependencies dependencies;

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

  dependencies.prepareSignals =
      [] {
        SignalHandler::reset();
        SignalHandler::install();
      };

  dependencies.sleep =
      [](std::chrono::seconds duration) {
        std::this_thread::sleep_for(duration);
      };

  return dependencies;
}

bool ClientRuntime::sleepInterruptibly(
    std::chrono::seconds duration) const {
  for (std::chrono::seconds elapsed{0};
       elapsed < duration;
       elapsed += std::chrono::seconds(1)) {
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
    if (dependencies_.stopRequested()) {
      std::cout << "openpuzzle stopped.\n";

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

  const auto reportDetectedSolution =
      [&] {
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
            << "Wallet file........ "
            << exported.walletPath
            << '\n'
            << "Format............. WIF (compressed)\n"
            << "Permissions........ owner only\n"
            << "Private key........ not displayed or uploaded\n";

        return true;
      };

  constexpr auto syncInterval =
      std::chrono::seconds(60);

  constexpr auto completionPollInterval =
      std::chrono::seconds(2);

  while (true) {
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
            << "Stopping BitCrack...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Warning............ unable to "
              << "confirm BitCrack termination\n";
        } else {
          std::cout
              << "BitCrack............ stopped\n";
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

      std::cout << "Stopping BitCrack...\n";

      if (!dependencies_.stopExecution(
              workspace)) {
        std::cerr
            << "Unable to stop BitCrack cleanly.\n";

        return 1;
      }

      std::cout
          << "BitCrack............ stopped\n";

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
      std::cout << "Assignment complete.\n";

      return 0;
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
            << "Stopping BitCrack...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Warning............ unable to "
              << "confirm BitCrack termination\n";
        } else {
          std::cout
              << "BitCrack............ stopped\n";
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
        std::cerr
            << "Assignment......... rejected by server\n"
            << "Stopping BitCrack...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Unable to stop BitCrack cleanly.\n";

          return 1;
        }

        if (!dependencies_.removeState()) {
          std::cerr
              << "Unable to remove rejected "
              << "execution state.\n";

          return 1;
        }

        std::cout
            << "BitCrack............ stopped\n"
            << "Local state........ removed\n";

        return 0;
      }

      if (
          result.progressStatus ==
          client::AssignmentUploadStatus::
              PermanentFailure) {
        std::cerr
            << "Progress error..... permanent\n"
            << "Stopping BitCrack...\n";

        if (!dependencies_.stopExecution(
                workspace)) {
          std::cerr
              << "Unable to stop BitCrack cleanly.\n";
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

    std::cout
        << "Completion......... uploaded\n"
        << "Assignment complete.\n";

    return 0;
  }
}

} // namespace openpuzzle
