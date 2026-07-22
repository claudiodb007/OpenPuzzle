#include "openpuzzle/runtime/RunSession.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/setup/FirstRunSetup.hpp"

#include "openpuzzle/client/ClientHeartbeatService.hpp"
#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/ClientRegistrationService.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/core/commands/BenchmarkCommand.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ClientRuntime.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/RunBenchmarkPreparation.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace openpuzzle {

namespace {

std::string getArgument(const std::vector<std::string> &args,
                        const std::string &name,
                        const std::string &fallback = {}) {
  for (std::size_t index = 0; index + 1 < args.size(); ++index) {
    if (args[index] == name) {
      return args[index + 1];
    }
  }

  return fallback;
}

int getIntegerArgument(const std::vector<std::string> &args,
                       const std::string &name, int fallback) {
  const auto value = getArgument(args, name);

  if (value.empty()) {
    return fallback;
  }

  return std::stoi(value);
}

bool hasArgument(const std::vector<std::string> &args,
                 const std::string &name) {
  for (const auto &argument : args) {
    if (argument == name) {
      return true;
    }
  }

  return false;
}

std::string selectedBackend(
    const std::vector<std::string> &args) {
  const auto configuration =
      ConfigurationManager::load();

  return getArgument(
      args,
      "--backend",
      configuration.engine.backend.empty()
          ? ToolManager::bundledBackend()
          : configuration.engine.backend);
}

int logicalProcessorCount() {
  const long processors =
      sysconf(_SC_NPROCESSORS_ONLN);

  return processors > 0
      ? static_cast<int>(processors)
      : 1;
}

int selectedCpuThreads(
    const std::vector<std::string> &args) {
  std::string value =
      getArgument(args, "--threads");

  if (value.empty()) {
    value = getArgument(args, "--t");
  }

  const int available =
      logicalProcessorCount();

  if (value.empty()) {
    if (
        hasArgument(args, "--threads") ||
        hasArgument(args, "--t")) {
      throw std::runtime_error(
          "CPU threads require a value between 1 and " +
          std::to_string(available));
    }

    return available;
  }

  std::size_t consumed = 0;
  long long requested = 0;

  try {
    requested =
        std::stoll(value, &consumed);
  } catch (...) {
    throw std::runtime_error(
        "CPU threads must be a whole number between 1 and " +
        std::to_string(available));
  }

  if (
      consumed != value.size() ||
      requested < 1 ||
      requested > available) {
    throw std::runtime_error(
        "CPU threads must be between 1 and " +
        std::to_string(available));
  }

  return static_cast<int>(requested);
}

std::string serverUrl(const std::vector<std::string> &args) {
  std::string value = "https://claudiodb.com";

  if (const char *environment = std::getenv("OPENPUZZLE_SERVER_URL")) {
    if (*environment != '\0') {
      value = environment;
    }
  }

  return getArgument(args, "--server", value);
}

std::optional<int> requestedDurationMinutes(
    const std::vector<std::string> &args) {
  std::string value;

  if (const char *environment =
          std::getenv(
              "OPENPUZZLE_TARGET_DURATION_MINUTES")) {
    if (*environment != '\0') {
      value = environment;
    }
  }

  value = getArgument(
      args,
      "--duration-minutes",
      value);

  if (value.empty()) {
    return std::nullopt;
  }

  std::size_t consumed = 0;
  const int duration =
      std::stoi(value, &consumed);

  if (consumed != value.size() ||
      duration < 1 ||
      duration > 360) {
    throw std::runtime_error(
        "Duration must be between 1 and 360 minutes");
  }

  return duration;
}

int selectedPuzzle(const std::vector<std::string> &args) {
  /*
   * Formato principal:
   *
   *   openpuzzle run 71
   *
   * --puzzle permanece temporariamente disponível
   * para compatibilidade com scripts antigos.
   */
  if (args.size() >= 2 && !args[1].empty() && args[1][0] != '-') {
    std::size_t consumed = 0;

    const int puzzle = std::stoi(args[1], &consumed);

    if (consumed != args[1].size() || puzzle <= 0) {
      throw std::runtime_error("Invalid puzzle number: " + args[1]);
    }

    return puzzle;
  }

  return getIntegerArgument(args, "--puzzle", 0);
}

std::optional<double> measuredSpeedMKeys(const std::vector<std::string> &args) {
  CommandContext context;

  if (!context.initialize()) {
    return std::nullopt;
  }

  const auto configuration = ConfigurationManager::load();

  const std::string configuredBackend = configuration.engine.backend.empty()
                                            ? "cuda"
                                            : configuration.engine.backend;

  const std::string backend = getArgument(args, "--backend", configuredBackend);

  const auto gpu = GpuManager::currentGpu(
      backend == "opencl"
          ? "OpenCL"
          : "CUDA");

  GpuProfileManager profiles(context.db);

  const auto profile = profiles.chooseBest(
      gpu.name, backend == "opencl" ? "OpenCL" : "CUDA", "BitCrack");

  if (!profile ||
      profile->blocks <= 0 ||
      profile->threads <= 0 ||
      profile->points <= 0 ||
      profile->averageSpeed <= 0.0) {
    return std::nullopt;
  }

  return profile->averageSpeed;
}

std::filesystem::path assignmentWorkspace(const std::string &assignmentId) {
  const char *home = std::getenv("HOME");

  const std::filesystem::path root =
      home ? std::filesystem::path(home) : std::filesystem::current_path();

  return root / ".local" / "share" / "OpenPuzzle" / "assignments" /
         assignmentId;
}

bool processExists(int pid) {
  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
}

ClientIterationResult monitoredResult(
    int exitCode) {
  if (
      exitCode ==
      ClientRuntime::SolutionFoundExitCode) {
    return
        ClientIterationResult::
            solutionFound();
  }

  return ClientIterationResult(
      exitCode);
}

void printAssignment(const client::RangeAssignment &assignment) {
  std::cout << "Assignment......... " << assignment.assignmentId << '\n'
            << "Puzzle............. " << assignment.puzzle << '\n'
            << "Assignment number... " << assignment.rangeId << '\n'
            << "Target............. " << assignment.target << '\n'
            << "Start.............. " << assignment.start << '\n'
            << "End................ " << assignment.end << '\n';
}

int showStatus(const std::vector<std::string> &args) {
  const std::string server = serverUrl(args);

  client::ExecutionSyncService syncService;

  const auto result = syncService.tick(server);

  std::cout << "OpenPuzzle Status\n"
            << "-----------------\n";

  if (!result.hasState) {
    const auto runtimePid =
        ClientRuntimeControl::runtimePid();

    if (runtimePid &&
        ClientRuntimeControl::running()) {
      std::cout
          << "Status............. waiting\n"
          << "Execution.......... none\n"
          << "Runtime PID........ "
          << *runtimePid
          << '\n';

      return 0;
    }

    std::cout
        << "Status............. idle\n"
        << "Execution.......... none\n";

    return 0;
  }

  const auto &state = result.state;

  std::cout << "Status............. "
            << (
                   result.solutionFound
                       ? "solution found"
                       : (
                             result.running
                                 ? "running"
                                 : "stopped"))
            << '\n'
            << "Assignment......... " << state.assignmentId << '\n'
            << "Puzzle............. " << state.puzzle << '\n'
            << "Assignment number... " << state.rangeId << '\n'
            << "PID................ " << state.pid << '\n'
            << "Engine............. " << state.engine << '\n'
            << "Backend............ " << state.backend << '\n'
            << "Start.............. " << state.start << '\n'
            << "End................ " << state.end << '\n'
            << "Workspace.......... " << state.workspace << '\n';

  if (result.solutionFound) {
    std::cout
        << "Solution file...... "
        << result.solutionPath
        << '\n'
        << "Local state........ preserved\n"
        << "Private key........ not displayed\n";

    return 0;
  }

  if (result.running) {
    if (result.hasProgress) {
      std::cout << "Speed.............. " << result.progress.speedMKeys
                << " MKey/s\n"
                << "Keys checked....... " << result.progress.keysChecked
                << '\n';

      if (result.progressUploaded) {
        std::cout << "Progress........... uploaded\n";
      } else {
        std::cerr << "Progress........... failed\n"
                  << "Upload error....... " << result.progressError << '\n';
      }
    } else {
      std::cout << "Progress........... "
                << "waiting for engine output\n";
    }

    return 0;
  }

  if (!result.hasExitCode) {
    return 0;
  }

  std::cout << "Exit code.......... " << result.exitCode << '\n';

  if (result.exitCode != 0) {
    if (!result.completionUploaded) {
      std::cerr
          << "Failure upload..... failed\n"
          << "Reason............. "
          << result.completionError
          << '\n';

      return 1;
    }

    std::cout
        << "Failure upload..... uploaded\n";

    if (!result.stateRemoved) {
      std::cerr
          << "State cleanup...... failed\n"
          << "Reason............. "
          << result.completionError
          << '\n';

      return 1;
    }

    std::cout
        << "Local state........ removed\n";

    return 0;
  }

  if (!result.completionUploaded) {
    std::cerr << "Completion......... failed\n"
              << "Upload error....... " << result.completionError << '\n';

    return 1;
  }

  std::cout << "Completion......... uploaded\n";

  if (!result.stateRemoved) {
    std::cerr << "Warning............ " << result.completionError << '\n';

    return 1;
  }

  return 0;
}

int stopExecution() {
  std::cout << "OpenPuzzle\n"
            << "----------\n";

  /*
   * O runtime principal é responsável por sincronizar,
   * cancelar a atribuição e terminar o motor.
   */
  if (ClientRuntimeControl::requestStop()) {
    std::cout
        << "Stop requested.\n"
        << "The active runtime is shutting down.\n";

    return 0;
  }

  const auto state =
      client::ClientStateStore::load();

  if (!state) {
    std::cout << "No active execution to stop.\n";

    return 0;
  }

  const auto solution =
      client::ExecutionSyncService::
          solutionFile(
              state->workspace);

  if (solution) {
    std::cout
        << "Solution found.\n"
        << "Solution file...... "
        << *solution
        << '\n'
        << "Local state was preserved.\n";

    return 0;
  }

  if (!processExists(state->pid)) {
    client::ClientStateStore::remove();

    std::cout << "Execution is no longer running.\n"
              << "Local state removed.\n";

    return 0;
  }

  ExecutionStopper stopper;

  if (!stopper.stop(state->workspace)) {
    std::cerr << "Unable to stop execution " << state->pid << '\n';

    return 1;
  }

  client::ClientStateStore::remove();

  std::cout << "Execution stopped.\n"
            << "Assignment......... " << state->assignmentId << '\n';

  return 0;
}

} // namespace

int RunSession::run(
    const std::vector<std::string> &args) const {
  /*
   * status, stop e claim são operações únicas.
   * Apenas run entra no ciclo contínuo.
   */
  if (args.empty() ||
      args.front() != "run" ||
      hasArgument(args, "--dry-run") ||
      hasArgument(args, "--once")) {
    return runOnce(args).exitCode;
  }

  ClientRuntime runtime;

  bool initializeClient = true;

  return runtime.runContinuous(
      serverUrl(args),
      [this, &args, &initializeClient] {
        const auto result =
            runOnce(
                args,
                initializeClient);

        initializeClient = false;

        return result;
      });
}

ClientIterationResult RunSession::runOnce(
    const std::vector<std::string> &args,
    bool initializeClient) const {
  if (args.empty()) {
    std::cerr << "Usage:\n"
              << "  openpuzzle run <puzzle> [--dry-run]\n"
              << "  openpuzzle status\n"
              << "  openpuzzle stop\n";

    return 1;
  }

  const std::string subcommand = args.front();

  if (subcommand == "status") {
    return showStatus(args);
  }

  if (subcommand == "stop") {
    return stopExecution();
  }

  if (subcommand != "claim" && subcommand != "run") {
    std::cerr << "Unknown range command: " << subcommand << '\n';

    return 1;
  }

  const std::string server =
      serverUrl(args);

  /*
   * Recuperar primeiro qualquer execução local
   * pertencente a uma sessão anterior.
   */
  if (subcommand == "run") {
    const auto existing = client::ClientStateStore::load();

    if (existing &&
        processExists(existing->pid)) {
      std::cout
          << "Recovering active execution...\n"
          << "PID................ "
          << existing->pid
          << '\n'
          << "Assignment......... "
          << existing->assignmentId
          << '\n'
          << "Workspace.......... "
          << existing->workspace
          << "\n\n";

      ClientRuntime recoveryRuntime;

      return monitoredResult(
          recoveryRuntime.run(
              server,
              existing->assignmentId,
              existing->clientId,
              existing->workspace));
    }

    if (existing) {
      std::cout
          << "Recovering finished execution...\n"
          << "Assignment......... "
          << existing->assignmentId
          << '\n';

      client::ExecutionSyncService syncService;

      const auto recovery =
          syncService.tick(server);

      if (recovery.solutionFound) {
        std::cout
            << "\n"
            << "SOLUTION FOUND\n"
            << "--------------\n"
            << "Solution file...... "
            << recovery.solutionPath
            << '\n'
            << "Workspace.......... "
            << existing->workspace
            << '\n'
            << "Local state........ preserved\n"
            << "The private key was not read "
            << "or uploaded by OpenPuzzle.\n";

        return
            ClientIterationResult::
                solutionFound();
      }

      if (!recovery.hasExitCode) {
        return ClientIterationResult::retry(
            "Execution stopped but exit.code "
            "is not available yet");
      }

      if (
          recovery.completionStatus ==
          client::AssignmentUploadStatus::
              AssignmentRejected) {
        if (!recovery.stateRemoved) {
          return ClientIterationResult::retry(
              recovery.completionError.empty()
                  ? "Unable to remove rejected "
                    "execution state"
                  : recovery.completionError);
        }

        std::cout
            << "Recovered assignment rejected "
            << "by server.\n"
            << "Local state........ removed\n\n";
      } else {
        if (
            recovery.completionStatus ==
            client::AssignmentUploadStatus::
                PermanentFailure) {
          std::cerr
              << "Recovered completion has a "
              << "permanent error.\n"
              << "Reason............. "
              << recovery.completionError
              << '\n'
              << "Local state retained for "
              << "diagnosis.\n";

          return 1;
        }

        if (!recovery.completionUploaded) {
          return ClientIterationResult::retry(
              recovery.completionError.empty()
                  ? "Unable to synchronize "
                    "finished execution"
                  : recovery.completionError);
        }

        if (!recovery.stateRemoved) {
          return ClientIterationResult::retry(
              recovery.completionError.empty()
                  ? "Unable to remove recovered "
                    "execution state"
                  : recovery.completionError);
        }

        if (recovery.exitCode != 0) {
          std::cerr
              << "Recovered failure.. uploaded\n"
              << "Exit code.......... "
              << recovery.exitCode
              << '\n';

          return recovery.exitCode;
        }

        std::cout
            << "Recovered completion uploaded.\n"
            << "Local state........ removed\n\n";
      }
    }
  }

  const int puzzleNumber =
      selectedPuzzle(args);

  const bool dryRun = hasArgument(args, "--dry-run");

  const std::string runBackend =
      subcommand == "run"
          ? selectedBackend(args)
          : "";

  if (
      subcommand == "run" &&
      !ToolManager::supportsBackend(
          runBackend)) {
    std::cerr
        << "This OpenPuzzle package does not support the "
        << runBackend
        << " backend.\n";

    return 1;
  }

  if (subcommand == "run" &&
      runBackend == "cpu") {
    try {
      (void) selectedCpuThreads(args);
    } catch (const std::exception &error) {
      std::cerr << error.what() << '\n';
      return 1;
    }

    if (!ToolManager::keyhuntPath()) {
      std::cerr
          << "The bundled KeyHunt CPU engine is "
          << "missing or invalid.\n";

      return 1;
    }
  }

  if (subcommand == "run" &&
      initializeClient &&
      runBackend != "cpu") {
    FirstRunSetup setup;

    if (!setup.ensureConfigured()) {
      return 1;
    }
  }

  if (subcommand == "run") {
    /*
     * A fresh run must have a validated local GPU
     * profile before registration, heartbeat or
     * assignment requests. Existing executions are
     * recovered above and dry-run remains local and
     * non-benchmarking.
     */
    if (
        initializeClient &&
        !dryRun &&
        runBackend != "cpu") {
      const int device =
          getIntegerArgument(
              args,
              "--device",
              GpuManager::selectedGpu());

      RunBenchmarkPreparationDependencies
          preparationDependencies;

      preparationDependencies.hasValidProfile =
          [&args] {
            return
                measuredSpeedMKeys(args)
                    .has_value();
          };

      preparationDependencies.runBenchmark =
          [&runBackend, device] {
            return BenchmarkCommand().run({
                "--real",
                "--auto",
                "--backend",
                runBackend,
                "--gpu",
                std::to_string(device),
            });
          };

      const RunBenchmarkPreparation preparation(
          preparationDependencies);

      if (!preparation.ensureProfile()) {
        return 1;
      }
    }
  }

  const auto requestedDuration =
      requestedDurationMinutes(args);

  int targetDurationMinutes =
      requestedDuration.value_or(60);

  double speedMKeys = 0.0;
  bool calibrationAssignment = false;

  if (subcommand == "run") {
    if (runBackend == "cpu") {
      /*
       * KeyHunt does not require a benchmark. Until a
       * completed CPU assignment provides live speed,
       * request a short range unless the user selected
       * a duration explicitly.
       */
      if (!requestedDuration) {
        targetDurationMinutes = 5;
      }
    } else {
      const auto measured =
          measuredSpeedMKeys(args);

      if (measured) {
        speedMKeys = *measured;
      } else {
        /*
         * Sem perfil medido, usar uma atribuição curta
         * de calibração, exceto quando o utilizador
         * definiu explicitamente a duração.
         */
        if (!requestedDuration) {
          targetDurationMinutes = 5;
        }

        calibrationAssignment = true;
      }
    }
  }

  const std::string clientId = client::ClientIdentity::loadOrCreate();

  if (clientId.empty()) {
    std::cerr << "Unable to create local client identity\n";

    return 1;
  }

  if (subcommand != "run" ||
      initializeClient) {
    std::cout << "OpenPuzzle\n"
              << "----------\n";

    if (subcommand == "run") {
      if (puzzleNumber > 0) {
        std::cout << "Requested puzzle... " << puzzleNumber << '\n';
      } else {
        std::cout << "Requested puzzle... automatic\n";
      }

      if (calibrationAssignment) {
        std::cout << "Assignment type.... calibration\n";
      }

      if (hasArgument(args, "--once")) {
        std::cout << "Execution mode..... single assignment\n";
      }

      std::cout << "Target duration.... " << targetDurationMinutes
                << " minutes\n";

      if (speedMKeys > 0.0) {
        std::cout << "Measured speed..... " << speedMKeys << " MKey/s\n";
      }
    }
  }

  /*
   * O dry-run é estritamente local.
   *
   * Não regista heartbeat, não reclama trabalho e
   * não cria leases ou assignments no servidor.
   */
  if (subcommand == "run" && dryRun) {
    if (runBackend == "cpu") {
      const int cpuThreads =
          selectedCpuThreads(args);

      std::cout << "\nLocal configuration\n"
                << "-------------------\n"
                << "CPU processors..... "
                << logicalProcessorCount()
                << '\n'
                << "Engine............. KeyHunt\n"
                << "Backend............ CPU\n"
                << "Executable......... "
                << *ToolManager::keyhuntPath()
                << '\n'
                << "Threads............ "
                << cpuThreads
                << "\n\n"
                << "Dry run only. "
                << "No assignment was requested.\n";

      return 0;
    }

    const auto configuration =
        ConfigurationManager::load();

    const std::string backend =
        runBackend;

    const auto executable =
        backend == "opencl"
            ? ToolManager::bitcrackOpenCLPath()
            : ToolManager::bitcrackCudaPath();

    const auto gpu = GpuManager::currentGpu(
      backend == "opencl"
          ? "OpenCL"
          : "CUDA");

    std::cout << "\nLocal configuration\n"
              << "-------------------\n"
              << "GPU................ " << gpu.name << '\n'
              << "Engine............. "
              << (configuration.engine.id.empty() ? "bitcrack"
                                                  : configuration.engine.id)
              << '\n'
              << "Backend............ "
              << (backend == "opencl" ? "OpenCL" : "CUDA") << '\n'
              << "Executable......... "
              << executable.value_or(
                     "(bundled engine unavailable)")
              << "\n\n"
              << "Dry run only. "
              << "No assignment was requested.\n";

    return 0;
  }

  /*
   * Mostrar primeiro a configuração local.
   * A ligação à rede acontece imediatamente depois.
   */
  if (subcommand == "run" &&
      initializeClient) {
    client::ClientRegistrationService registrationService;

    const auto registration = registrationService.registerWith(server);

    if (!registration.success) {
      std::cerr << "Unable to connect to the "
                << "OpenPuzzle network: " << registration.error << '\n';

      return 1;
    }

    client::ClientHeartbeatService heartbeatService;

    const auto heartbeat = heartbeatService.send(server);

    if (!heartbeat.success) {
      std::cerr << "Unable to update client status: " << heartbeat.error
                << '\n';

      return 1;
    }
  }

  std::cout << "Requesting assignment...\n\n";

  client::HttpRangeClient httpClient(server);

  const auto claimResult =
      httpClient.claimResult(
          clientId,
          puzzleNumber,
          targetDurationMinutes,
          speedMKeys);

  if (claimResult.unavailable()) {
    if (subcommand == "claim") {
      std::cout
          << "No assignment available.\n";

      if (!claimResult.message.empty()) {
        std::cout
            << "Reason............. "
            << claimResult.message
            << '\n';
      }

      return 0;
    }

    return ClientIterationResult::unavailable(
        claimResult.message);
  }

  if (claimResult.failed() ||
      !claimResult.assignment) {
    if (subcommand == "claim") {
      std::cerr
          << "Unable to claim assignment: "
          << claimResult.message
          << '\n';

      return 1;
    }

    return ClientIterationResult::retry(
        claimResult.message);
  }

  const auto assignment =
      claimResult.assignment;

  printAssignment(*assignment);

  if (subcommand == "claim") {
    return 0;
  }

  CommandContext context;

  if (
      runBackend != "cpu" &&
      !context.initialize()) {
    std::cerr << context.lastError() << '\n';

    return 1;
  }

  const bool cpuBackend =
      runBackend == "cpu";

  const std::string engine =
      getArgument(
          args,
          "--engine",
          cpuBackend
              ? "keyhunt"
              : "bitcrack");

  if (
      (cpuBackend && engine != "keyhunt") ||
      (!cpuBackend && engine != "bitcrack")) {
    throw std::runtime_error(
        "The " + runBackend +
        " backend requires the " +
        (cpuBackend
             ? "KeyHunt"
             : "BitCrack") +
        " engine");
  }

  int device =
      cpuBackend
          ? 0
          : getIntegerArgument(
                args,
                "--device",
                context.gpu);

  int blocks =
      cpuBackend
          ? 0
          : getIntegerArgument(
                args,
                "--blocks",
                getIntegerArgument(
                    args,
                    "--b",
                    256));

  int threads =
      cpuBackend
          ? selectedCpuThreads(args)
          : getIntegerArgument(
                args,
                "--threads",
                getIntegerArgument(
                    args,
                    "--t",
                    256));

  int points =
      cpuBackend
          ? 0
          : getIntegerArgument(
                args,
                "--points",
                getIntegerArgument(
                    args,
                    "--p",
                    256));

  const bool manualProfile =
      hasArgument(args, "--blocks") || hasArgument(args, "--b") ||
      hasArgument(args, "--threads") || hasArgument(args, "--t") ||
      hasArgument(args, "--points") || hasArgument(args, "--p");

  std::string hardwareLabel = "CPU processors";
  std::string hardwareValue =
      std::to_string(
          logicalProcessorCount());

  if (!cpuBackend) {
    const auto gpu =
        GpuManager::currentGpu(
            runBackend == "opencl"
                ? "OpenCL"
                : "CUDA");

    hardwareLabel = "GPU";
    hardwareValue = gpu.name;
  }

  if (!cpuBackend && !manualProfile) {
    GpuProfileManager profiles(context.db);

    const auto gpu =
        GpuManager::currentGpu(
            runBackend == "opencl"
                ? "OpenCL"
                : "CUDA");

    const auto profile = profiles.chooseBest(
        gpu.name,
        runBackend == "opencl"
            ? "OpenCL"
            : "CUDA",
        "BitCrack");

    if (profile) {
      blocks = profile->blocks;

      threads = profile->threads;

      points = profile->points;

      std::cout << "\nGPU profile found\n";
    } else {
      std::cout << "\nNo GPU profile available; "
                << "using defaults\n";
    }
  }

  std::string executable;

  if (cpuBackend) {
    const auto keyhunt =
        ToolManager::keyhuntPath();

    if (!keyhunt) {
      throw std::runtime_error(
          "KeyHunt CPU executable not configured");
    }

    executable = *keyhunt;
  } else if (runBackend == "cuda") {
    if (!context.bitcrack) {
      throw std::runtime_error("BitCrack CUDA executable not configured");
    }

    executable = *context.bitcrack;
  } else {
    const auto opencl = ToolManager::bitcrackOpenCLPath();

    if (!opencl) {
      throw std::runtime_error("BitCrack OpenCL executable not configured");
    }

    executable = *opencl;
  }

  PuzzleRecord puzzle;

  puzzle.id = assignment->puzzle;

  puzzle.number = assignment->puzzle;

  puzzle.name = "Puzzle " + std::to_string(assignment->puzzle);

  puzzle.address = assignment->target;

  puzzle.rangeStart = assignment->start;

  puzzle.rangeEnd = assignment->end;

  RangeRecord range;

  range.id = assignment->rangeId;

  range.puzzleId = puzzle.id;

  range.startKey = assignment->start;

  range.endKey = assignment->end;

  range.status = RangeStatus::Running;

  JobRecord job;

  job.id = assignment->rangeId;

  job.puzzleId = puzzle.id;

  job.rangeId = range.id;

  job.state = JobState::Running;

  WorkerEngineCapability capability;

  capability.engine =
      cpuBackend
          ? "KeyHunt"
          : "BitCrack";

  capability.backend =
      cpuBackend
          ? "CPU"
          : (
                runBackend == "opencl"
                    ? "OpenCL"
                    : "CUDA");

  capability.device = device;

  capability.blocks = blocks;

  capability.threads = threads;

  capability.points = points;

  capability.available = true;

  const auto workspace = assignmentWorkspace(assignment->assignmentId);

  WorkspaceSecurity::prepare(
      workspace);

  EngineManager engineManager;

  ExecutionRequestBuilder builder(engineManager);

  auto request = builder.build(puzzle, range, job, capability, executable,
                               workspace.string());

  request.executionId = assignment->rangeId;

  std::cout << "\nLocal configuration\n"
            << "-------------------\n"
            << std::left
            << std::setw(20)
            << (hardwareLabel + "...")
            << ' '
            << hardwareValue
            << '\n'
            << "Engine............. " << capability.engine << '\n'
            << "Backend............ " << capability.backend << '\n'
            << "Executable......... " << executable << '\n'
            << "Threads............ " << threads << '\n'
            << "Workspace.......... " << request.workspace << '\n';

  if (!cpuBackend) {
    std::cout
        << "Device............. " << device << '\n'
        << "Blocks............. " << blocks << '\n'
        << "Points............. " << points << '\n';
  }

  std::cout << "\n"
            << "Command\n"
            << "-------\n"
            << request.command << "\n\n";

  if (dryRun) {
    std::cout
        << "Dry run only. "
        << capability.engine
        << " was not started.\n";

    return 0;
  }

  BackgroundExecutionLauncher launcher;

  const auto handle = launcher.start(request);

  client::ClientExecutionState state;

  state.active = true;
  state.assignmentId = assignment->assignmentId;
  state.clientId = clientId;

  state.puzzle = assignment->puzzle;
  state.rangeId = assignment->rangeId;
  state.pid = handle.pid;

  state.target = assignment->target;
  state.start = assignment->start;
  state.end = assignment->end;

  state.engine = capability.engine;
  state.backend = capability.backend;

  state.workspace = handle.workspace;
  state.command = request.command;

  if (!client::ClientStateStore::save(state)) {
    ExecutionStopper stopper;

    stopper.stop(handle.workspace);

    throw std::runtime_error("Unable to save local execution state");
  }

  std::cout << capability.engine << " started.\n"
            << "PID................ " << handle.pid << '\n'
            << "Monitoring.......... active\n\n";

  ClientRuntime runtime;

  return monitoredResult(
      runtime.run(
          server,
          assignment->assignmentId,
          clientId,
          handle.workspace));
}

} // namespace openpuzzle
