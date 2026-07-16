#include "openpuzzle/runtime/ClientRuntime.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace openpuzzle;

namespace {

ClientRuntimeDependencies makeDependencies() {
  ClientRuntimeDependencies dependencies;

  dependencies.sync =
      [](const std::string &) {
        return client::ExecutionSyncResult{};
      };

  dependencies.heartbeat =
      [](const std::string &) {
        client::ClientHeartbeatResult result;
        result.success = true;
        return result;
      };

  dependencies.stopExecution =
      [](const std::string &) {
        return true;
      };

  dependencies.finalizeAssignment =
      [](const std::string &,
         const std::string &,
         const std::string &,
         int,
         const std::string &,
         std::string &) {
        return true;
      };

  dependencies.removeState =
      [] {
        return true;
      };

  dependencies.acquireRuntime =
      [] {
        return true;
      };

  dependencies.releaseRuntime =
      [] {};

  dependencies.stopRequested =
      [] {
        return false;
      };

  dependencies.prepareSignals =
      [] {};

  dependencies.sleep =
      [](std::chrono::seconds) {};

  return dependencies;
}

client::ExecutionSyncResult completedResult() {
  client::ExecutionSyncResult result;

  result.hasState = true;
  result.hasExitCode = true;
  result.exitCode = 0;
  result.completionUploaded = true;
  result.stateRemoved = true;

  return result;
}

} // namespace

int main() {
  /*
   * Conclusão bem-sucedida.
   */
  {
    auto dependencies =
        makeDependencies();

    int syncCalls = 0;

    dependencies.sync =
        [&](const std::string &server) {
          assert(server == "https://server.test");
          ++syncCalls;
          return completedResult();
        };

    ClientRuntime runtime(
        std::move(dependencies));

    assert(
        runtime.run(
            "https://server.test",
            "assignment-1",
            "client-1",
            "/tmp/workspace-1") == 0);

    assert(syncCalls == 1);
  }

  /*
   * Execução ativa: sincroniza progresso,
   * envia heartbeat e depois conclui.
   */
  {
    auto dependencies =
        makeDependencies();

    int syncCalls = 0;
    int heartbeatCalls = 0;
    int sleepCalls = 0;

    dependencies.sync =
        [&](const std::string &) {
          ++syncCalls;

          if (syncCalls == 1) {
            client::ExecutionSyncResult result;
            result.hasState = true;
            result.running = true;
            result.hasProgress = true;
            result.progressUploaded = true;
            result.progress.speedMKeys = 1250.5;
            result.progress.keysChecked = "2500000";
            return result;
          }

          return completedResult();
        };

    dependencies.heartbeat =
        [&](const std::string &) {
          ++heartbeatCalls;
          client::ClientHeartbeatResult result;
          result.success = true;
          return result;
        };

    dependencies.sleep =
        [&](std::chrono::seconds duration) {
          assert(duration == std::chrono::seconds(1));
          ++sleepCalls;
        };

    ClientRuntime runtime(
        std::move(dependencies));

    assert(
        runtime.run(
            "https://server.test",
            "assignment-2",
            "client-2",
            "/tmp/workspace-2") == 0);

    assert(syncCalls == 2);
    assert(heartbeatCalls == 1);
    assert(sleepCalls == 60);
  }

  /*
   * Cancelamento solicitado por sinal.
   */
  {
    auto dependencies =
        makeDependencies();

    bool stopped = false;
    bool cancelled = false;
    bool stateRemoved = false;

    dependencies.stopRequested =
        [] {
          return true;
        };

    dependencies.sync =
        [](const std::string &) {
          client::ExecutionSyncResult result;
          result.hasState = true;
          return result;
        };

    dependencies.stopExecution =
        [&](const std::string &workspace) {
          assert(workspace == "/tmp/workspace-3");
          stopped = true;
          return true;
        };

    dependencies.finalizeAssignment =
        [&](const std::string &server,
            const std::string &assignment,
            const std::string &clientId,
            int exitCode,
            const std::string &status,
            std::string &) {
          assert(server == "https://server.test");
          assert(assignment == "assignment-3");
          assert(clientId == "client-3");
          assert(exitCode == -2);
          assert(status == "cancelled");
          cancelled = true;
          return true;
        };

    dependencies.removeState =
        [&] {
          stateRemoved = true;
          return true;
        };

    ClientRuntime runtime(
        std::move(dependencies));

    assert(
        runtime.run(
            "https://server.test",
            "assignment-3",
            "client-3",
            "/tmp/workspace-3") == 0);

    assert(stopped);
    assert(cancelled);
    assert(stateRemoved);
  }

  /*
   * Exit code diferente de zero:
   * reporta failed e remove o estado.
   */
  {
    auto dependencies =
        makeDependencies();

    bool failureUploaded = false;
    bool stateRemoved = false;

    dependencies.sync =
        [](const std::string &) {
          client::ExecutionSyncResult result;
          result.hasState = true;
          result.hasExitCode = true;
          result.exitCode = 7;
          return result;
        };

    dependencies.finalizeAssignment =
        [&](const std::string &server,
            const std::string &assignment,
            const std::string &clientId,
            int exitCode,
            const std::string &status,
            std::string &) {
          assert(server == "https://server.test");
          assert(assignment == "assignment-4");
          assert(clientId == "client-4");
          assert(exitCode == 7);
          assert(status == "failed");

          failureUploaded = true;
          return true;
        };

    dependencies.removeState =
        [&] {
          stateRemoved = true;
          return true;
        };

    ClientRuntime runtime(
        std::move(dependencies));

    assert(
        runtime.run(
            "https://server.test",
            "assignment-4",
            "client-4",
            "/tmp/workspace-4") == 1);

    assert(failureUploaded);
    assert(stateRemoved);
  }

  /*
   * Falha temporária no upload da conclusão:
   * deve voltar a tentar.
   */
  {
    auto dependencies =
        makeDependencies();

    int syncCalls = 0;
    int sleepCalls = 0;

    dependencies.sync =
        [&](const std::string &) {
          ++syncCalls;

          if (syncCalls == 1) {
            auto result =
                completedResult();

            result.completionUploaded = false;
            result.stateRemoved = false;
            result.completionError =
                "temporary failure";

            return result;
          }

          return completedResult();
        };

    dependencies.sleep =
        [&](std::chrono::seconds duration) {
          assert(duration == std::chrono::seconds(1));
          ++sleepCalls;
        };

    ClientRuntime runtime(
        std::move(dependencies));

    assert(
        runtime.run(
            "https://server.test",
            "assignment-5",
            "client-5",
            "/tmp/workspace-5") == 0);

    assert(syncCalls == 2);
    assert(sleepCalls == 60);
  }

  /*
   * O ciclo pede uma nova atribuição depois
   * de cada conclusão.
   */
  {
    auto dependencies =
        makeDependencies();

    int executions = 0;

    dependencies.stopRequested =
        [&] {
          return executions >= 2;
        };

    ClientRuntime runtime(
        std::move(dependencies));

    const int result =
        runtime.runContinuous(
            [&] {
              ++executions;
              return ClientIterationResult{};
            });

    assert(result == 0);
    assert(executions == 2);
  }

  /*
   * Ausência temporária de trabalho:
   * aguarda e volta a tentar.
   */
  {
    auto dependencies =
        makeDependencies();

    int executions = 0;
    int sleepCalls = 0;

    dependencies.stopRequested =
        [&] {
          return executions >= 2;
        };

    dependencies.sleep =
        [&](std::chrono::seconds duration) {
          assert(
              duration ==
              std::chrono::seconds(1));

          ++sleepCalls;
        };

    ClientRuntime runtime(
        std::move(dependencies));

    const int result =
        runtime.runContinuous(
            [&] {
              ++executions;

              if (executions == 1) {
                return
                    ClientIterationResult::unavailable(
                        "No work available");
              }

              return ClientIterationResult{};
            });

    assert(result == 0);
    assert(executions == 2);
    assert(sleepCalls == 30);
  }

  /*
   * Falha local permanente termina o runtime.
   */
  {
    auto dependencies =
        makeDependencies();

    ClientRuntime runtime(
        std::move(dependencies));

    const int result =
        runtime.runContinuous(
            [] {
              return ClientIterationResult(7);
            });

    assert(result == 7);
  }

  std::cout
      << "ClientRuntimeTests passed\n";

  return 0;
}
