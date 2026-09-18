#include "openpuzzle/runtime/RunSession.hpp"
#include "openpuzzle/client/SolutionExporter.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/setup/FirstRunSetup.hpp"
#include "openpuzzle/services/PuzzleMetadataCatalog.hpp"
#include "openpuzzle/engines/common/PuzzleExecutionPlanner.hpp"

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
#include "openpuzzle/hardware/RusticlEnvironment.hpp"
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ClientRuntime.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/CudaDeviceSelection.hpp"
#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/engines/common/SearchMode.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/KangarooCheckpointRecoveryFlow.hpp"
#include "openpuzzle/runtime/KangarooWalkSeed.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include "openpuzzle/runtime/RunBenchmarkPreparation.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <cerrno>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
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

int selectedGpuDevice(
    const std::vector<std::string> &args,
    int fallback) {
  const std::string value =
      getArgument(
          args,
          "--device");

  if (value.empty()) {
    return fallback;
  }

  std::size_t consumed = 0;
  long long parsed = -1;

  try {
    parsed = std::stoll(value, &consumed);
  } catch (...) {
    throw std::runtime_error(
        "GPU device must be a "
        "non-negative whole number");
  }

  if (
      consumed != value.size() ||
      parsed < 0 ||
      parsed > 2147483647LL) {
    throw std::runtime_error(
        "GPU device must be a "
        "non-negative whole number");
  }

  return static_cast<int>(parsed);
}

const GpuInfo *findGpuDevice(
    const std::vector<GpuInfo> &devices,
    int device) {
  const auto found =
      std::find_if(
          devices.begin(),
          devices.end(),
          [device](const GpuInfo &gpu) {
            return gpu.device == device;
          });

  return found == devices.end()
      ? nullptr
      : &*found;
}

std::string normalizedGpuName(
    const std::string &name) {
  std::string result;

  for (const char character : name) {
    const auto value =
        static_cast<unsigned char>(character);

    if (std::isalnum(value)) {
      result.push_back(
          static_cast<char>(
              std::tolower(value)));
    }
  }

  return result;
}

void requireUsableGpuDevice(
    const std::vector<GpuInfo> &devices,
    int device,
    const std::string &backend) {
  const auto *gpu =
      findGpuDevice(
          devices,
          device);

  if (!gpu) {
    throw std::runtime_error(
        backend +
        " device " +
        std::to_string(device) +
        " was not found");
  }

  if (
      gpu->name.empty() ||
      gpu->memoryMb <= 0) {
    throw std::runtime_error(
        backend +
        " device " +
        std::to_string(device) +
        " did not report usable hardware details");
  }
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

std::string selectedRusticlSelector(
    const std::vector<std::string> &args) {
  const auto configuration =
      ConfigurationManager::load();

  return getArgument(
      args,
      "--rusticl-enable",
      configuration.gpu.rusticlEnable);
}

int logicalProcessorCount() {
  const long processors =
      sysconf(_SC_NPROCESSORS_ONLN);

  return processors > 0
      ? static_cast<int>(processors)
      : 1;
}

std::vector<std::string>
buildConcurrentArguments(
    const std::vector<std::string>& args,
    bool cpu) {
  const std::string cpuThreads =
      getArgument(
          args,
          "--cpu-threads");

  if (cpu && cpuThreads.empty()) {
    throw std::runtime_error(
        "--with-cpu requires "
        "--cpu-threads <number>");
  }

  std::vector<std::string> result;

  const auto cpuOnlyOption =
      [](const std::string& argument) {
        return
            argument == "--backend" ||
            argument == "--device" ||
            argument == "--gpu" ||
            argument == "--rusticl-enable" ||
            argument == "--blocks" ||
            argument == "--b" ||
            argument == "--threads" ||
            argument == "--t" ||
            argument == "--points" ||
            argument == "--p";
      };

  for (std::size_t index = 0;
       index < args.size();
       ++index) {
    const auto& argument =
        args[index];

    if (argument == "--with-cpu") {
      continue;
    }

    if (argument == "--cpu-threads") {
      if (index + 1 >= args.size()) {
        throw std::runtime_error(
            "--cpu-threads requires a value");
      }

      ++index;
      continue;
    }

    if (cpu &&
        cpuOnlyOption(argument)) {
      if (index + 1 >= args.size()) {
        throw std::runtime_error(
            argument +
            " requires a value");
      }

      ++index;
      continue;
    }

    result.push_back(argument);
  }

  if (cpu) {
    result.push_back("--backend");
    result.push_back("cpu");
    result.push_back("--threads");
    result.push_back(cpuThreads);
  }

  return result;
}

std::vector<std::string>
buildCudaOpenclArguments(
    const std::vector<std::string>& args,
    bool opencl) {
  if (selectedBackend(args) != "cuda") {
    throw std::runtime_error(
        "--with-opencl requires "
        "--backend cuda");
  }

  const std::string openclDevice =
      getArgument(
          args,
          "--opencl-device");

  if (openclDevice.empty()) {
    throw std::runtime_error(
        "--with-opencl requires "
        "--opencl-device <number>");
  }

  std::size_t consumed = 0;
  long long parsedDevice = -1;

  try {
    parsedDevice =
        std::stoll(
            openclDevice,
            &consumed);
  } catch (...) {
    throw std::runtime_error(
        "OpenCL device must be a "
        "non-negative whole number");
  }

  if (
      consumed != openclDevice.size() ||
      parsedDevice < 0) {
    throw std::runtime_error(
        "OpenCL device must be a "
        "non-negative whole number");
  }

  const std::string rusticlDrivers =
      selectedRusticlSelector(args);

  std::vector<std::string> result;

  for (std::size_t index = 0;
       index < args.size();
       ++index) {
    const auto& argument = args[index];

    if (argument == "--with-opencl") {
      continue;
    }

    if (
        argument == "--opencl-device" ||
        argument == "--rusticl-enable") {
      if (index + 1 >= args.size()) {
        throw std::runtime_error(
            argument + " requires a value");
      }

      ++index;
      continue;
    }

    if (
        opencl &&
        (
            argument == "--backend" ||
            argument == "--device" ||
            argument == "--gpu"
        )) {
      if (index + 1 >= args.size()) {
        throw std::runtime_error(
            argument + " requires a value");
      }

      ++index;
      continue;
    }

    result.push_back(argument);
  }

  if (opencl) {
    result.push_back("--backend");
    result.push_back("opencl");
    result.push_back("--device");
    result.push_back(openclDevice);

    if (!rusticlDrivers.empty()) {
      result.push_back("--rusticl-enable");
      result.push_back(rusticlDrivers);
    }
  }

  return result;
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

  const int device =
      getIntegerArgument(
          args,
          "--device",
          GpuManager::selectedGpu());

  const auto gpu = GpuManager::currentGpu(
      backend == "opencl"
          ? "OpenCL"
          : "CUDA",
      device);

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

bool processIdentityMatches(
    const client::ClientExecutionState& state) {
  if (state.bootId.empty() ||
      state.processStartTime == 0) {
    return false;
  }

  const auto currentBootId =
      client::ClientStateStore::
          currentBootId();

  if (currentBootId.empty() ||
      state.bootId != currentBootId) {
    return false;
  }

  if (!processExists(state.pid)) {
    return false;
  }

  const auto currentStartTime =
      LinuxProcessIdentity::
          startTime(state.pid);

  return
      currentStartTime &&
      *currentStartTime ==
          state.processStartTime;
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
  (void)args;

  const std::vector<std::string>
      executionSlots = {
          "gpu",
          "cpu",
          "cuda",
          "opencl",
      };

  bool slotStateFound = false;

  for (const auto& slot :
       executionSlots) {
    if (
        client::ClientStateStore::
            load(slot) ||
        ClientRuntimeControl::
            running(slot)) {
      slotStateFound = true;
      break;
    }
  }

  if (slotStateFound) {
    std::cout
        << "OpenPuzzle Status\n"
        << "-----------------\n";

    client::ExecutionSyncService
        syncService;

    for (const auto& slot :
         executionSlots) {
      const auto result =
          syncService.inspect(slot);

      const auto runtimePid =
          ClientRuntimeControl::
              runtimePid(slot);

      if (
          !result.hasState &&
          !(
              runtimePid &&
              ClientRuntimeControl::
                  running(slot)
          )) {
        continue;
      }

      std::cout
          << "\nSlot............... "
          << slot
          << '\n';

      if (!result.hasState) {
        std::cout
            << "Status............. "
            << (
                   runtimePid &&
                           ClientRuntimeControl::
                               running(slot)
                       ? "waiting"
                       : "idle")
            << '\n'
            << "Execution.......... none\n";

        continue;
      }

      const auto& state =
          result.state;

      std::cout
          << "Status............. "
          << (
                 result.solutionFound
                     ? "solution found"
                     : (
                           result.running
                               ? "running"
                               : "stopped"))
          << '\n'
          << "Assignment......... "
          << state.assignmentId
          << '\n'
          << "Puzzle............. "
          << state.puzzle
          << '\n'
          << "Assignment number... "
          << state.rangeId
          << '\n'
          << "PID................ "
          << state.pid
          << '\n'
          << "Engine............. "
          << state.engine
          << '\n'
          << "Backend............ "
          << state.backend
          << '\n'
          << "Start.............. "
          << state.start
          << '\n'
          << "End................ "
          << state.end
          << '\n'
          << "Workspace.......... "
          << state.workspace
          << '\n';

      if (
          state.backend == "CPU" ||
          state.backend == "cpu") {
        std::cout
            << "Threads............ "
            << state.threads
            << '\n';
      }

      if (result.solutionFound) {
        std::cout
            << "Solution file...... "
            << result.solutionPath
            << '\n'
            << "Local state........ preserved\n"
            << "Private key........ not displayed\n";

        continue;
      }

      if (result.running) {
        if (result.hasProgress) {
          std::cout
              << "Speed.............. "
              << result.progress.speedMKeys
              << " MKey/s\n";

          const auto statusEngine =
              normalizedGpuName(state.engine);

          if (statusEngine == "kangaroo" ||
              statusEngine == "psckangaroo") {
            std::cout
                << "Linear coverage.... not applicable\n";
          } else {
            std::cout
                << "Keys checked....... "
                << result.progress.keysChecked
                << '\n';
          }

          std::cout
              << "Progress........... runtime managed\n";
        } else {
          std::cout
              << "Progress........... "
              << "waiting for engine output\n";
        }

        continue;
      }

      if (!result.hasExitCode) {
        continue;
      }

      std::cout
          << "Exit code.......... "
          << result.exitCode
          << '\n';

      std::cout
          << "Synchronization.... pending supervisor\n";
    }

    return 0;
  }

  client::ExecutionSyncService syncService;

  const auto result = syncService.inspect();

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
      std::cout
          << "Speed.............. "
          << result.progress.speedMKeys
          << " MKey/s\n";

      const auto statusEngine =
          normalizedGpuName(state.engine);

      if (statusEngine == "kangaroo" ||
          statusEngine == "psckangaroo") {
        std::cout
            << "Linear coverage.... not applicable\n";
      } else {
        std::cout
            << "Keys checked....... "
            << result.progress.keysChecked
            << '\n';
      }

      std::cout
          << "Progress........... runtime managed\n";
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

  std::cout
      << "Synchronization.... pending supervisor\n";

  return 0;
}

int stopExecution() {
  std::cout << "OpenPuzzle\n"
            << "----------\n";

  bool concurrentStopRequested =
      false;

  for (const std::string slot : {
           "gpu",
           "cpu",
           "cuda",
           "opencl",
       }) {
    if (
        ClientRuntimeControl::
            requestStop(slot)) {
      concurrentStopRequested = true;

      std::cout
          << "Stop requested..... "
          << slot
          << '\n';
    }
  }

  if (concurrentStopRequested) {
    std::cout
        << "The active runtimes are "
        << "shutting down.\n";

    return 0;
  }

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
              state->workspace,
              state->engine);

  if (solution) {
    std::cout
        << "Solution found.\n"
        << "Solution file...... "
        << *solution
        << '\n'
        << "Local state was preserved.\n";

    return 0;
  }

  if (!processIdentityMatches(*state)) {
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

int safeStopExecution() {
  std::cout << "OpenPuzzle Safe Stop\n"
            << "--------------------\n";

  std::vector<std::string> activeSlots;

  for (const std::string slot : {
           "gpu",
           "cpu",
           "cuda",
           "opencl",
       }) {
    if (
        ClientRuntimeControl::
            running(slot)) {
      activeSlots.push_back(slot);
    }
  }

  if (!activeSlots.empty()) {
    std::vector<std::string> requestedSlots;

    for (const auto& slot : activeSlots) {
      if (!ClientRuntimeControl::
               requestSafeStop(slot)) {
        for (const auto& requested :
             requestedSlots) {
          ClientRuntimeControl::
              clearSafeStop(requested);
        }

        std::cerr
            << "Unable to request safe stop for "
            << slot
            << ".\n"
            << "No partial safe-stop request "
            << "was retained.\n";

        return 1;
      }

      requestedSlots.push_back(slot);

      std::cout
          << "Safe stop requested. "
          << slot
          << '\n';
    }

    std::cout
        << "Current ranges...... will finish\n"
        << "New assignments..... blocked\n"
        << "Runtime exit........ automatic\n";

    return 0;
  }

  if (ClientRuntimeControl::running()) {
    if (!ClientRuntimeControl::
             requestSafeStop()) {
      std::cerr
          << "Unable to create the safe-stop "
          << "request.\n";

      return 1;
    }

    std::cout
        << "Safe stop requested. primary\n"
        << "Current range....... will finish\n"
        << "New assignment...... blocked\n"
        << "Runtime exit........ automatic\n";

    return 0;
  }

  std::cout
      << "No active OpenPuzzle runtime.\n";

  return 0;
}

int runConcurrent(
    const std::vector<std::string>& args) {
  const bool withCpu =
      hasArgument(args, "--with-cpu");

  const bool withOpencl =
      hasArgument(args, "--with-opencl");

  if (withCpu == withOpencl) {
    throw std::runtime_error(
        "Concurrent execution requires exactly "
        "one of --with-cpu or --with-opencl");
  }

  std::string requestedEngine =
      getArgument(args, "--engine");

  std::transform(
      requestedEngine.begin(),
      requestedEngine.end(),
      requestedEngine.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  bool kangarooWorkload =
      requestedEngine == "kangaroo";

  if (requestedEngine.empty()) {
    const int puzzle = selectedPuzzle(args);
    const auto metadata =
        puzzle > 0
            ? PuzzleMetadataCatalog::load(puzzle)
            : std::nullopt;

    kangarooWorkload =
        metadata &&
        metadata->searchMode == "kangaroo" &&
        selectedBackend(args) == "cuda";
  }

  if (kangarooWorkload) {
    throw std::runtime_error(
        "Kangaroo execution is exclusive; "
        "remove --with-cpu or --with-opencl");
  }

  const std::string firstSlot =
      withOpencl ? "cuda" : "gpu";

  const std::string secondSlot =
      withOpencl ? "opencl" : "cpu";

  const std::string firstLabel =
      withOpencl ? "CUDA" : "GPU";

  const std::string secondLabel =
      withOpencl ? "OpenCL" : "CPU";

  const auto firstArguments =
      withOpencl
          ? RunSession::
                concurrentCudaArguments(args)
          : RunSession::
                concurrentGpuArguments(args);

  const auto secondArguments =
      withOpencl
          ? RunSession::
                concurrentOpenclArguments(args)
          : RunSession::
                concurrentCpuArguments(args);

  const std::vector<std::string> allSlots = {
      "primary",
      "gpu",
      "cpu",
      "cuda",
      "opencl",
      "cuda",
      "opencl",
  };

  for (const auto& slot : allSlots) {
    if (ClientRuntimeControl::running(slot)) {
      std::cerr
          << "An OpenPuzzle runtime is already "
          << "active in slot "
          << slot
          << ".\n";

      return 1;
    }
  }

  if (withOpencl) {
    const std::string rusticlDrivers =
        getArgument(
            args,
            "--rusticl-enable");

    if (!rusticlDrivers.empty()) {
      RusticlEnvironment::apply(
          rusticlDrivers);
    }

    try {
      RunSession::
          validateConcurrentGpuSelection(
              firstArguments,
              secondArguments,
              GpuManager::listCudaGpus(),
              GpuManager::listOpenClGpus());
    } catch (const std::exception &error) {
      std::cerr
          << "OpenPuzzle concurrent GPU preflight failed\n"
          << "------------------------------------------\n"
          << "Error code......... OP-GPU-001\n"
          << "Problem............ "
          << error.what()
          << '\n'
          << "Assignment......... not requested\n"
          << "Action 1........... run: "
          << "openpuzzle doctor --offline\n"
          << "Action 2........... verify --device and "
          << "--opencl-device\n";

      return 1;
    }
  }

  const auto preflight =
      [](const std::string &label,
         const std::vector<std::string> &childArguments) {
        const int result =
            RunSession().run(
                RunSession::
                    concurrentPreflightArguments(
                        childArguments));

        if (result != 0) {
          std::cerr
              << "Concurrent "
              << label
              << " preflight failed.\n"
              << "Assignment......... not requested\n";
        }

        return result == 0;
      };

  if (!preflight(
          firstLabel,
          firstArguments)) {
    return 1;
  }

  if (!preflight(
          secondLabel,
          secondArguments)) {
    return 1;
  }

  const auto launch =
      [](const std::string& slot,
         const std::vector<std::string>&
             childArguments) {
        const pid_t pid = fork();

        if (pid != 0) {
          return pid;
        }

        if (
            setenv(
                "OPENPUZZLE_EXECUTION_SLOT",
                slot.c_str(),
                1) != 0) {
          _exit(1);
        }

        const int result =
            RunSession().run(
                childArguments);

        std::cout.flush();
        std::cerr.flush();
        _exit(result);
      };

  const pid_t firstPid =
      launch(
          firstSlot,
          firstArguments);

  if (firstPid < 0) {
    std::cerr
        << "Unable to start "
        << firstLabel
        << " runtime.\n";

    return 1;
  }

  const pid_t secondPid =
      launch(
          secondSlot,
          secondArguments);

  if (secondPid < 0) {
    kill(firstPid, SIGTERM);
    waitpid(firstPid, nullptr, 0);

    std::cerr
        << "Unable to start "
        << secondLabel
        << " runtime.\n";

    return 1;
  }

  std::cout
      << "OpenPuzzle concurrent execution\n"
      << "-------------------------------\n"
      << firstLabel
      << " runtime PID.... "
      << firstPid
      << '\n'
      << secondLabel
      << " runtime PID.... "
      << secondPid
      << '\n';

  if (withCpu) {
    std::cout
        << "CPU threads........ "
        << getArgument(
               args,
               "--cpu-threads")
        << '\n';
  } else {
    std::cout
        << "OpenCL device...... "
        << getArgument(
               args,
               "--opencl-device")
        << '\n';
  }

  std::cout << '\n';

  const bool finiteRun =
      hasArgument(args, "--once") ||
      hasArgument(args, "--dry-run");

  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  if (finiteRun) {
    int firstStatus = 0;
    int secondStatus = 0;

    waitpid(firstPid, &firstStatus, 0);
    waitpid(secondPid, &secondStatus, 0);

    return
        (
            WIFEXITED(firstStatus) &&
            WEXITSTATUS(firstStatus) == 0 &&
            WIFEXITED(secondStatus) &&
            WEXITSTATUS(secondStatus) == 0
        )
            ? 0
            : 1;
  }

  int completedStatus = 0;

  const pid_t completedPid =
      waitpid(-1, &completedStatus, 0);

  const pid_t remainingPid =
      completedPid == firstPid
          ? secondPid
          : firstPid;

  const std::string remainingSlot =
      completedPid == firstPid
          ? secondSlot
          : firstSlot;

  if (remainingPid > 0) {
    if (
        ClientRuntimeControl::
            safeStopRequested(
                remainingSlot)) {
      int remainingStatus = 0;

      if (
          waitpid(
              remainingPid,
              &remainingStatus,
              0) != remainingPid) {
        return 1;
      }

      return
          (
              completedPid > 0 &&
              WIFEXITED(completedStatus) &&
              WEXITSTATUS(completedStatus) == 0 &&
              WIFEXITED(remainingStatus) &&
              WEXITSTATUS(remainingStatus) == 0
          )
              ? 0
              : 1;
    }

    kill(remainingPid, SIGTERM);
    waitpid(remainingPid, nullptr, 0);
  }

  if (
      completedPid < 0 ||
      !WIFEXITED(completedStatus)) {
    return 1;
  }

  return WEXITSTATUS(completedStatus);
}

} // namespace

std::vector<std::string>
RunSession::concurrentGpuArguments(
    const std::vector<std::string>& args) {
  return buildConcurrentArguments(
      args,
      false);
}

std::vector<std::string>
RunSession::concurrentCpuArguments(
    const std::vector<std::string>& args) {
  const auto result =
      buildConcurrentArguments(
          args,
          true);

  (void) selectedCpuThreads(result);

  return result;
}


std::vector<std::string>
RunSession::concurrentCudaArguments(
    const std::vector<std::string>& args) {
  return buildCudaOpenclArguments(
      args,
      false);
}

std::vector<std::string>
RunSession::concurrentOpenclArguments(
    const std::vector<std::string>& args) {
  return buildCudaOpenclArguments(
      args,
      true);
}

std::vector<std::string>
RunSession::concurrentPreflightArguments(
    const std::vector<std::string>& args) {
  auto result = args;

  if (!hasArgument(
          result,
          "--preflight-only")) {
    result.push_back(
        "--preflight-only");
  }

  return result;
}

std::vector<int>
RunSession::selectedCudaDevices(
    const std::vector<std::string>& args,
    const std::vector<GpuInfo>& availableDevices) {
  const auto occurrences =
      static_cast<std::size_t>(
          std::count(
              args.begin(),
              args.end(),
              "--devices"));

  if (occurrences != 1) {
    throw std::runtime_error(
        occurrences == 0
            ? "--devices is required for multi-CUDA selection"
            : "--devices may only be specified once");
  }

  if (hasArgument(args, "--device") ||
      hasArgument(args, "--gpu")) {
    throw std::runtime_error(
        "--devices cannot be combined with --device or --gpu");
  }

  if (hasArgument(args, "--with-cpu") ||
      hasArgument(args, "--with-opencl")) {
    throw std::runtime_error(
        "--devices cannot be combined with "
        "--with-cpu or --with-opencl");
  }

  const auto backend =
      getArgument(args, "--backend", "cuda");

  if (backend != "cuda") {
    throw std::runtime_error(
        "--devices requires --backend cuda");
  }

  const auto engine =
      getArgument(args, "--engine", "bitcrack");

  if (engine != "bitcrack") {
    throw std::runtime_error(
        "--devices currently requires --engine bitcrack");
  }

  return CudaDeviceSelection::resolve(
      getArgument(args, "--devices"),
      availableDevices);
}

void RunSession::validateConcurrentGpuSelection(
    const std::vector<std::string> &cudaArguments,
    const std::vector<std::string> &openclArguments,
    const std::vector<GpuInfo> &cudaDevices,
    const std::vector<GpuInfo> &openclDevices) {
  const int cudaDevice =
      selectedGpuDevice(
          cudaArguments,
          GpuManager::selectedGpu());

  const int openclDevice =
      selectedGpuDevice(
          openclArguments,
          GpuManager::selectedGpu());

  requireUsableGpuDevice(
      cudaDevices,
      cudaDevice,
      "CUDA");

  requireUsableGpuDevice(
      openclDevices,
      openclDevice,
      "OpenCL");

  const auto *cudaGpu =
      findGpuDevice(
          cudaDevices,
          cudaDevice);

  const auto *openclGpu =
      findGpuDevice(
          openclDevices,
          openclDevice);

  const std::string cudaName =
      normalizedGpuName(cudaGpu->name);

  const std::string openclName =
      normalizedGpuName(openclGpu->name);

  if (
      !cudaName.empty() &&
      cudaName == openclName) {
    throw std::runtime_error(
        "CUDA and OpenCL resolve to the same "
        "physical GPU: " +
        cudaGpu->name);
  }
}

int RunSession::run(
    const std::vector<std::string> &args) const {
  if (
      !args.empty() &&
      args.front() == "run" &&
      (
          hasArgument(
              args,
              "--with-cpu") ||
          hasArgument(
              args,
              "--with-opencl")
      )) {
    try {
      return runConcurrent(args);
    } catch (const std::exception& error) {
      std::cerr
          << error.what()
          << '\n';

      return 1;
    }
  }

  /*
   * status, stop, safestop e claim são operações únicas.
   * Apenas run entra no ciclo contínuo.
   */
  if (args.empty() ||
      args.front() != "run" ||
      hasArgument(args, "--dry-run") ||
      hasArgument(args, "--preflight-only") ||
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
              << "  openpuzzle stop\n"
              << "  openpuzzle safestop\n";

    return 1;
  }

  const std::string subcommand = args.front();

  if (subcommand == "status") {
    return showStatus(args);
  }

  if (subcommand == "stop") {
    return stopExecution();
  }

  if (subcommand == "safestop") {
    return safeStopExecution();
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
    bool rejectedKangarooRecovery = false;

    if (existing &&
        processIdentityMatches(*existing)) {
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

    /*
     * Result material always takes precedence over checkpoint recovery.
     * A power failure may happen after PSCKangaroo writes RESULTS.TXT but
     * before the launcher copies it to found.txt or sends the metadata-only
     * report. Re-enter ClientRuntime so export and report retry use the same
     * audited path as a solution detected during normal execution.
     */
    if (existing &&
        client::ExecutionSyncService::solutionFile(
            existing->workspace,
            existing->engine)) {
      std::cout
          << "Recovering protected solution...\n"
          << "Assignment......... "
          << existing->assignmentId
          << "\n\n";

      ClientRuntime recoveryRuntime;

      return monitoredResult(
          recoveryRuntime.run(
              server,
              existing->assignmentId,
              existing->clientId,
              existing->workspace));
    }

    const auto existingEngine =
        existing
            ? normalizedGpuName(existing->engine)
            : std::string{};

    if (existing &&
        (existingEngine == "kangaroo" ||
         existingEngine == "psckangaroo")) {
      std::cout
          << "Recovering interrupted Kangaroo execution...\n"
          << "Assignment......... "
          << existing->assignmentId
          << '\n';

      const auto executable =
          ToolManager::kangarooPath();

      if (!executable) {
        std::cerr
            << "Kangaroo recovery.. blocked\n"
            << "Reason............. PSCKangaroo executable is unavailable\n"
            << "Local state........ preserved for diagnosis\n";

        return 1;
      }

      const auto recovery =
          KangarooCheckpointRecoveryFlow::recover(
              server,
              *existing,
              *executable,
              assignmentWorkspace(existing->assignmentId));

      switch (recovery.status) {
      case KangarooCheckpointRecoveryFlowStatus::Resumed: {
        std::cout
            << "Kangaroo recovery.. resumed\n"
            << "PID................ "
            << recovery.state.pid
            << "\n\n";

        ClientRuntime recoveryRuntime;

        return monitoredResult(
            recoveryRuntime.run(
                server,
                recovery.state.assignmentId,
                recovery.state.clientId,
                recovery.state.workspace));
      }

      case KangarooCheckpointRecoveryFlowStatus::Retry:
        return ClientIterationResult::retry(
            recovery.error.empty()
                ? "Unable to verify the Kangaroo assignment lease"
                : recovery.error);

      case KangarooCheckpointRecoveryFlowStatus::Rejected:
        if (!client::ClientStateStore::remove()) {
          return ClientIterationResult::retry(
              "Unable to remove rejected Kangaroo execution state");
        }

        rejectedKangarooRecovery = true;

        std::cout
            << "Kangaroo recovery.. rejected by server\n"
            << "Checkpoint......... preserved\n"
            << "Local state........ removed\n\n";
        break;

      case KangarooCheckpointRecoveryFlowStatus::Invalid:
        std::cerr
            << "Kangaroo recovery.. blocked\n"
            << "Reason............. "
            << (recovery.error.empty()
                    ? "unsafe or incomplete local recovery state"
                    : recovery.error)
            << '\n'
            << "Local state........ preserved for diagnosis\n";

        return 1;
      }
    }

    if (existing && !rejectedKangarooRecovery) {
      std::cout
          << "Recovering finished execution...\n"
          << "Assignment......... "
          << existing->assignmentId
          << '\n';

      client::ExecutionSyncService syncService;

      const auto recovery =
          syncService.tick(server);

      if (recovery.solutionFound) {
        ClientRuntime recoveryRuntime;

        return monitoredResult(
            recoveryRuntime.run(
                server,
                recovery.state.assignmentId,
                recovery.state.clientId,
                recovery.state.workspace));
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

        if (recovery.interrupted) {
          std::cerr
              << "Recovered interruption\n"
              << "Cause.............. engine stopped "
                 "without exit.code\n"
              << "Assignment......... cancelled\n"
              << "Last progress...... preserved\n"
              << "Local state........ removed\n\n";
        } else {
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
  }

  const int puzzleNumber =
      selectedPuzzle(args);

  const bool dryRun = hasArgument(args, "--dry-run");

  const bool preflightOnly =
      hasArgument(
          args,
          "--preflight-only");

  const auto puzzleMetadata =
      subcommand == "run" && puzzleNumber > 0
          ? PuzzleMetadataCatalog::load(puzzleNumber)
          : std::nullopt;

  if (
      subcommand == "run" &&
      puzzleNumber > 0 &&
      !puzzleMetadata) {
    std::cerr
        << "OpenPuzzle puzzle metadata validation failed\n"
        << "--------------------------------------------\n"
        << "Puzzle............. " << puzzleNumber << '\n'
        << "Assignment......... not requested\n"
        << "Problem............ puzzle metadata is unavailable or invalid\n";

    return 1;
  }

  std::string requestedEngine =
      getArgument(args, "--engine");

  std::transform(
      requestedEngine.begin(),
      requestedEngine.end(),
      requestedEngine.begin(),
      [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });

  if (
      subcommand == "run" &&
      !requestedEngine.empty() &&
      requestedEngine != "kangaroo" &&
      requestedEngine != "bitcrack" &&
      requestedEngine != "keyhunt") {
    std::cerr
        << "Unsupported engine: " << requestedEngine << '\n'
        << "Use kangaroo, bitcrack or keyhunt.\n";
    return 1;
  }

  const bool puzzleSupportsKangaroo =
      puzzleMetadata && puzzleMetadata->searchMode == "kangaroo";

  const bool explicitLinearBackend =
      hasArgument(args, "--backend") &&
      selectedBackend(args) != "cuda";

  const bool kangarooWorkload =
      puzzleSupportsKangaroo &&
      (
          requestedEngine == "kangaroo" ||
          (
              requestedEngine.empty() &&
              !explicitLinearBackend
          )
      );

  if (
      subcommand == "run" &&
      requestedEngine == "kangaroo" &&
      !puzzleSupportsKangaroo) {
    std::cerr
        << "OpenPuzzle engine validation failed\n"
        << "-----------------------------------\n"
        << "Puzzle............. " << puzzleNumber << '\n'
        << "Assignment......... not requested\n"
        << "Problem............ this puzzle does not support Kangaroo\n";
    return 1;
  }

  const std::string runBackend =
      subcommand != "run"
          ? ""
          : (
                kangarooWorkload
                    ? "cuda"
                    : (
                          requestedEngine == "keyhunt"
                              ? "cpu"
                              : selectedBackend(args)
                      )
            );

  if (
      subcommand == "run" &&
      requestedEngine == "keyhunt" &&
      hasArgument(args, "--backend") &&
      selectedBackend(args) != "cpu") {
    std::cerr
        << "KeyHunt requires --backend cpu.\n";
    return 1;
  }

  if (
      subcommand == "run" &&
      requestedEngine == "bitcrack" &&
      runBackend == "cpu") {
    std::cerr
        << "BitCrack requires --backend cuda or opencl.\n";
    return 1;
  }

  if (
      subcommand == "run" &&
      kangarooWorkload &&
      hasArgument(args, "--backend") &&
      selectedBackend(args) != "cuda") {
    std::cerr
        << "OpenPuzzle engine validation failed\n"
        << "-----------------------------------\n"
        << "Puzzle............. " << puzzleNumber << '\n'
        << "Search mode........ Kangaroo\n"
        << "Required backend... CUDA\n"
        << "Assignment......... not requested\n"
        << "Problem............ Kangaroo requires CUDA\n";

    return 1;
  }

  const int runDevice =
      subcommand == "run" &&
              runBackend != "cpu"
          ? getIntegerArgument(
                args,
                "--device",
                GpuManager::selectedGpu())
          : 0;

  if (
      subcommand == "run" &&
      runDevice < 0) {
    std::cerr
        << "GPU device must not be negative.\n";

    return 1;
  }

  const std::string rusticlSelector =
      subcommand == "run" && runBackend == "opencl"
          ? selectedRusticlSelector(args)
          : "";

  if (
      subcommand == "run" &&
      hasArgument(args, "--rusticl-enable") &&
      runBackend != "opencl") {
    std::cerr
        << "--rusticl-enable requires the "
        << "OpenCL backend.\n";
    return 1;
  }

  if (
      subcommand == "run" &&
      runBackend == "opencl" &&
      !rusticlSelector.empty()) {
    try {
      RusticlEnvironment::apply(rusticlSelector);
    } catch (const std::exception &error) {
      std::cerr << error.what() << '\n';
      return 1;
    }
  }

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

  if (
      subcommand == "run" &&
      runBackend != "cpu") {
    try {
      const auto devices =
          runBackend == "opencl"
              ? GpuManager::listOpenClGpus()
              : GpuManager::listCudaGpus();

      requireUsableGpuDevice(
          devices,
          runDevice,
          runBackend == "opencl"
              ? "OpenCL"
              : "CUDA");
    } catch (const std::exception &error) {
      std::cerr
          << "OpenPuzzle GPU validation failed\n"
          << "--------------------------------\n"
          << "Error code......... OP-GPU-001\n"
          << "Problem............ "
          << error.what()
          << '\n'
          << "Assignment......... not requested\n"
          << "Action 1........... run: "
          << "openpuzzle doctor --offline\n"
          << "Action 2........... verify --device\n";

      return 1;
    }
  }

  if (
      subcommand == "run" &&
      kangarooWorkload) {
    const auto plan =
        PuzzleExecutionPlanner::plan(
            *puzzleMetadata,
            true,
            false,
            false,
            ToolManager::kangarooPath().has_value());

    std::cout
        << "\nOpenPuzzle puzzle routing\n"
        << "-------------------------\n"
        << "Puzzle............. " << puzzleNumber << '\n'
        << "Search mode........ Kangaroo\n"
        << "Engine............. Kangaroo\n"
        << "Backend............ CUDA\n"
        << "Public key......... "
        << (puzzleMetadata->publicKey.empty() ? "missing" : "available")
        << '\n';

    if (!plan.executable) {
      std::cerr
          << "Status............. NOT READY\n"
          << "Assignment......... not requested\n"
          << "Problem............ " << plan.reason << '\n';
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

  if (
      subcommand == "run" &&
      runBackend == "opencl" &&
      hasArgument(args, "--rusticl-enable") &&
      !dryRun &&
      !preflightOnly) {
    auto configuration = ConfigurationManager::load();
    if (configuration.gpu.rusticlEnable != rusticlSelector) {
      configuration.gpu.rusticlEnable = rusticlSelector;
      if (!ConfigurationManager::save(configuration)) {
        std::cerr << "Unable to persist the Rusticl selector.\n";
        return 1;
      }
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
        !kangarooWorkload &&
        runBackend != "cpu") {
      RunBenchmarkPreparationDependencies
          preparationDependencies;

      preparationDependencies.hasValidProfile =
          [&args] {
            return
                measuredSpeedMKeys(args)
                    .has_value();
          };

      preparationDependencies.runBenchmark =
          [&runBackend, runDevice] {
            return BenchmarkCommand().run({
                "--real",
                "--auto",
                "--backend",
                runBackend,
                "--gpu",
                std::to_string(runDevice),
            });
          };

      const RunBenchmarkPreparation preparation(
          preparationDependencies);

      if (!preparation.ensureProfile()) {
        return 1;
      }
    }
  }

  if (
      subcommand == "run" &&
      preflightOnly) {
    std::cout
        << "\nOpenPuzzle local preflight\n"
        << "--------------------------\n"
        << "Backend............ "
        << (
              runBackend == "opencl"
                  ? "OpenCL"
                  : (
                        runBackend == "cpu"
                            ? "CPU"
                            : "CUDA"
                    )
           )
        << '\n';

    if (runBackend != "cpu") {
      std::cout
          << "Device............. "
          << runDevice
          << '\n';
    }

    std::cout
        << "Status............. READY\n"
        << "Assignment......... not requested\n\n";

    return 0;
  }

  const auto requestedDuration =
      requestedDurationMinutes(args);

  int targetDurationMinutes =
      requestedDuration.value_or(60);

  double speedMKeys = 0.0;
  bool calibrationAssignment = false;

  if (
      subcommand == "run" &&
      runBackend != "cpu") {
    const auto measured =
        kangarooWorkload
            ? std::optional<double>{}
            : measuredSpeedMKeys(args);

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
        kangarooWorkload
            ? ToolManager::kangarooPath()
            : (
                  backend == "opencl"
                      ? ToolManager::bitcrackOpenCLPath()
                      : ToolManager::bitcrackCudaPath());

    const auto gpu = GpuManager::currentGpu(
      backend == "opencl"
          ? "OpenCL"
          : "CUDA",
      runDevice);

    std::cout << "\nLocal configuration\n"
              << "-------------------\n"
              << "GPU................ " << gpu.name << '\n'
              << "Engine............. "
              << (kangarooWorkload
                      ? "kangaroo"
                      : (configuration.engine.id.empty()
                             ? "bitcrack"
                             : configuration.engine.id))
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
          speedMKeys,
          runBackend,
          kangarooWorkload
              ? "kangaroo"
              : "linear");

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

  const auto assignmentSearchMode =
      searchModeFromString(
          assignment->searchMode);

  const bool kangarooAssignment =
      assignmentSearchMode == SearchMode::Kangaroo;

  if (kangarooAssignment != kangarooWorkload) {
    throw std::runtime_error(
        "Assignment search mode does not match validated puzzle metadata");
  }

  if (
      kangarooAssignment &&
      (
          runBackend != "cuda" ||
          (
              !assignment->requiredBackend.empty() &&
              assignment->requiredBackend != "cuda"))) {
    throw std::runtime_error(
        "Kangaroo assignment requires the CUDA backend");
  }

  const std::string expectedEngine =
      kangarooAssignment
          ? "kangaroo"
          : (cpuBackend ? "keyhunt" : "bitcrack");

  const std::string engine =
      getArgument(
          args,
          "--engine",
          expectedEngine);

  if (engine != expectedEngine) {
    throw std::runtime_error(
        "The " + runBackend +
        " backend requires the " +
        (kangarooAssignment
             ? "Kangaroo"
             : (cpuBackend ? "KeyHunt" : "BitCrack")) +
        " engine for this assignment");
  }

  int device =
      cpuBackend
          ? 0
          : runDevice;

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
                : "CUDA",
            device);

    hardwareLabel = "GPU";
    hardwareValue = gpu.name;
  }

  if (!cpuBackend && !kangarooAssignment && !manualProfile) {
    GpuProfileManager profiles(context.db);

    const auto gpu =
        GpuManager::currentGpu(
            runBackend == "opencl"
                ? "OpenCL"
                : "CUDA",
            device);

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

  if (kangarooAssignment) {
    const auto kangaroo =
        ToolManager::kangarooPath();

    if (!kangaroo) {
      throw std::runtime_error(
          "PSCKangaroo CUDA executable not configured; run: "
          "openpuzzle engine install psckangaroo");
    }

    executable = *kangaroo;
  } else if (cpuBackend) {
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

  puzzle.publicKey = assignment->publicKey;
  puzzle.searchMode = assignment->searchMode;
  puzzle.requiredBackend = assignment->requiredBackend;

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
      kangarooAssignment
          ? "Kangaroo"
          : (cpuBackend ? "KeyHunt" : "BitCrack");

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

  const auto walkSeed =
      kangarooAssignment
          ? KangarooWalkSeed::generate()
          : std::string{};

  EngineManager engineManager;

  ExecutionRequestBuilder builder(engineManager);

  auto request = builder.build(puzzle, range, job, capability, executable,
                               workspace.string(), walkSeed);

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
  state.bootId =
      client::ClientStateStore::currentBootId();

  if (state.bootId.empty()) {
    ExecutionStopper stopper;
    stopper.stop(handle.workspace);

    throw std::runtime_error(
        "Unable to determine Linux boot identity");
  }

  const auto processStartTime =
      LinuxProcessIdentity::startTime(
          handle.pid);

  if (!processStartTime ||
      *processStartTime == 0) {
    ExecutionStopper stopper;
    stopper.stop(handle.workspace);

    throw std::runtime_error(
        "Unable to determine Linux process start identity");
  }

  state.processStartTime =
      *processStartTime;

  state.device = device;
  state.blocks = blocks;
  state.threads = threads;
  state.points = points;
  state.profileManaged =
      !cpuBackend && !kangarooAssignment && !manualProfile;
  state.gpuName =
      cpuBackend ? std::string{} : hardwareValue;

  state.target = assignment->target;
  state.publicKey = assignment->publicKey;
  state.kangarooWalkSeed = request.walkSeed;
  state.kangarooGeneration = 0;
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
