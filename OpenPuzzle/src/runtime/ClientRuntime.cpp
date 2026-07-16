#include "openpuzzle/runtime/ClientRuntime.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
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
      !dependencies_.cancelAssignment ||
      !dependencies_.removeState ||
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

  dependencies.cancelAssignment =
      [](const std::string &serverUrl,
         const std::string &assignmentId,
         const std::string &clientId,
         std::string &error) {
        client::HttpRangeClient httpClient(
            serverUrl);

        const bool result =
            httpClient.complete(
                assignmentId,
                clientId,
                -2,
                "cancelled");

        if (!result) {
          error = httpClient.lastError();
        }

        return result;
      };

  dependencies.removeState =
      [] {
        return client::ClientStateStore::remove();
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

int ClientRuntime::run(
    const std::string &serverUrl,
    const std::string &assignmentId,
    const std::string &clientId,
    const std::string &workspace) const {
  dependencies_.prepareSignals();

  constexpr auto syncInterval =
      std::chrono::seconds(60);

  constexpr auto completionPollInterval =
      std::chrono::seconds(2);

  while (true) {
    if (dependencies_.stopRequested()) {
      std::cout << "\nStopping search...\n";

      const auto finalSync =
          dependencies_.sync(serverUrl);

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

      std::string cancellationError;

      if (!dependencies_.cancelAssignment(
              serverUrl,
              assignmentId,
              clientId,
              cancellationError)) {
        std::cerr
            << "Cancellation upload failed.\n"
            << "Reason............. "
            << cancellationError
            << '\n';

        return 1;
      }

      if (!dependencies_.removeState()) {
        std::cerr
            << "Unable to remove local "
            << "execution state.\n";

        return 1;
      }

      std::cout
          << "Assignment......... cancelled\n"
          << "Goodbye.\n";

      return 0;
    }

    const auto result =
        dependencies_.sync(serverUrl);

    if (!result.hasState) {
      std::cout << "Assignment complete.\n";

      return 0;
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

    if (result.exitCode != 0) {
      std::cerr
          << "Assignment......... failed\n"
          << "Local state retained for diagnosis.\n";

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
