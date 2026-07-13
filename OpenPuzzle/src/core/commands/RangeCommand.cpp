#include "openpuzzle/core/commands/RangeCommand.hpp"

#include "openpuzzle/adapters/bitcrack/BitCrackProgressParser.hpp"
#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace openpuzzle {

namespace {

std::string getArgument(
    const std::vector<std::string>& args,
    const std::string& name,
    const std::string& fallback = {}) {
  for (std::size_t index = 0;
       index + 1 < args.size();
       ++index) {
    if (args[index] == name) {
      return args[index + 1];
    }
  }

  return fallback;
}

int getIntegerArgument(
    const std::vector<std::string>& args,
    const std::string& name,
    int fallback) {
  const auto value =
      getArgument(
          args,
          name);

  if (value.empty()) {
    return fallback;
  }

  return std::stoi(value);
}

bool hasArgument(
    const std::vector<std::string>& args,
    const std::string& name) {
  for (const auto& argument : args) {
    if (argument == name) {
      return true;
    }
  }

  return false;
}

std::string serverUrl(
    const std::vector<std::string>& args) {
  std::string value =
      "https://claudiodb.com";

  if (const char* environment =
          std::getenv(
              "OPENPUZZLE_SERVER_URL")) {
    if (*environment != '\0') {
      value = environment;
    }
  }

  return getArgument(
      args,
      "--server",
      value);
}

std::filesystem::path assignmentWorkspace(
    const std::string& assignmentId) {
  const char* home =
      std::getenv("HOME");

  const std::filesystem::path root =
      home
          ? std::filesystem::path(home)
          : std::filesystem::current_path();

  return root /
         ".local" /
         "share" /
         "OpenPuzzle" /
         "assignments" /
         assignmentId;
}

bool processExists(
    int pid) {
  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
}

int readExitCode(
    const std::string& workspace) {
  const auto exitPath =
      std::filesystem::path(workspace) /
      "exit.code";

  std::ifstream input(
      exitPath);

  if (!input) {
    return -9999;
  }

  int exitCode = -9999;

  input >> exitCode;

  return exitCode;
}

std::optional<ExecutionProgress>
readLatestProgress(
    const std::string& workspace) {
  const auto logPath =
      std::filesystem::path(workspace) /
      "bitcrack.log";

  std::ifstream input(
      logPath);

  if (!input) {
    return std::nullopt;
  }

  bitcrack::BitCrackProgressParser parser;

  std::optional<ExecutionProgress>
      latest;

  std::string line;

  while (std::getline(
      input,
      line)) {
    const auto progress =
        parser.parseLine(line);

    if (!progress) {
      continue;
    }

    /*
     * Para telemetria pública consideramos apenas
     * eventos de velocidade/progresso.
     *
     * Eventos Found podem conter material sensível
     * e nunca são devolvidos por esta função.
     */
    if (progress->speedMKeys > 0.0 &&
        !progress->keysChecked.empty()) {
      latest =
          *progress;
    }
  }

  return latest;
}

void printAssignment(
    const client::RangeAssignment& assignment) {
  std::cout
      << "Assignment......... "
      << assignment.assignmentId
      << '\n'
      << "Puzzle............. "
      << assignment.puzzle
      << '\n'
      << "Range ID........... "
      << assignment.rangeId
      << '\n'
      << "Target............. "
      << assignment.target
      << '\n'
      << "Start.............. "
      << assignment.start
      << '\n'
      << "End................ "
      << assignment.end
      << '\n';
}

int showStatus(
    const std::vector<std::string>& args) {
  const std::string server =
      serverUrl(args);

  const auto state =
      client::ClientStateStore::load();

  std::cout
      << "OpenPuzzle Status\n"
      << "-----------------\n"
      << "Server............. "
      << server
      << '\n';

  if (!state) {
    std::cout
        << "Status............. idle\n"
        << "Execution.......... none\n";

    return 0;
  }

  const bool running =
      processExists(
          state->pid);

  const int exitCode =
      running
          ? -9999
          : readExitCode(
                state->workspace);

  std::cout
      << "Status............. "
      << (running
              ? "running"
              : "stopped")
      << '\n'
      << "Assignment......... "
      << state->assignmentId
      << '\n'
      << "Puzzle............. "
      << state->puzzle
      << '\n'
      << "Range ID........... "
      << state->rangeId
      << '\n'
      << "PID................ "
      << state->pid
      << '\n'
      << "Engine............. "
      << state->engine
      << '\n'
      << "Backend............ "
      << state->backend
      << '\n'
      << "Start.............. "
      << state->start
      << '\n'
      << "End................ "
      << state->end
      << '\n'
      << "Workspace.......... "
      << state->workspace
      << '\n';

  if (running) {
    const auto progress =
        readLatestProgress(
            state->workspace);

    if (progress) {
      std::cout
          << "Speed.............. "
          << progress->speedMKeys
          << " MKey/s\n"
          << "Keys checked....... "
          << progress->keysChecked
          << '\n';

      client::HttpRangeClient httpClient(
          server);

      if (httpClient.progress(
              state->assignmentId,
              state->clientId,
              progress->speedMKeys,
              progress->keysChecked)) {
        std::cout
            << "Progress........... uploaded\n";
      } else {
        std::cerr
            << "Progress........... failed\n"
            << "Upload error....... "
            << httpClient.lastError()
            << '\n';
      }
    } else {
      std::cout
          << "Progress........... waiting for engine output\n";
    }
  }

  if (!running &&
      exitCode != -9999) {
    std::cout
        << "Exit code.......... "
        << exitCode
        << '\n';

    if (exitCode == 0) {
      client::HttpRangeClient httpClient(
          server);

      if (httpClient.complete(
              state->assignmentId,
              state->clientId,
              exitCode)) {
        std::cout
            << "Completion......... uploaded\n";

        if (!client::ClientStateStore::remove()) {
          std::cerr
              << "Warning............ "
              << "unable to remove local state\n";

          return 1;
        }
      } else {
        std::cerr
            << "Completion......... failed\n"
            << "Upload error....... "
            << httpClient.lastError()
            << '\n';

        return 1;
      }
    } else {
      std::cout
          << "Completion......... "
          << "not uploaded\n"
          << "Reason............. "
          << "execution did not finish successfully\n";
    }
  }

  return 0;
}

int stopExecution() {
  const auto state =
      client::ClientStateStore::load();

  std::cout
      << "OpenPuzzle\n"
      << "----------\n";

  if (!state) {
    std::cout
        << "No active execution to stop.\n";

    return 0;
  }

  if (!processExists(
          state->pid)) {
    client::ClientStateStore::remove();

    std::cout
        << "Execution is no longer running.\n"
        << "Local state removed.\n";

    return 0;
  }

  ExecutionStopper stopper;

  if (!stopper.stop(
          state->workspace)) {
    std::cerr
        << "Unable to stop execution "
        << state->pid
        << '\n';

    return 1;
  }

  client::ClientStateStore::remove();

  std::cout
      << "Execution stopped.\n"
      << "Assignment......... "
      << state->assignmentId
      << '\n';

  return 0;
}

} // namespace

int RangeCommand::run(
    const std::vector<std::string>& args) const {
  if (args.empty()) {
    std::cerr
        << "Usage:\n"
        << "  OpenPuzzle run [--dry-run]\n"
        << "  OpenPuzzle status\n"
        << "  OpenPuzzle stop\n";

    return 1;
  }

  const std::string subcommand =
      args.front();

  if (subcommand == "status") {
    return showStatus(args);
  }

  if (subcommand == "stop") {
    return stopExecution();
  }

  if (subcommand != "claim" &&
      subcommand != "run") {
    std::cerr
        << "Unknown range command: "
        << subcommand
        << '\n';

    return 1;
  }

  /*
   * Não iniciar outra execução enquanto existir
   * um processo local ativo.
   */
  if (subcommand == "run") {
    const auto existing =
        client::ClientStateStore::load();

    if (existing &&
        processExists(
            existing->pid)) {
      std::cerr
          << "OpenPuzzle is already running.\n"
          << "PID................ "
          << existing->pid
          << '\n'
          << "Assignment......... "
          << existing->assignmentId
          << '\n';

      return 1;
    }

    if (existing) {
      client::ClientStateStore::remove();
    }
  }

  const std::string server =
      serverUrl(args);

  const int puzzleNumber =
      getIntegerArgument(
          args,
          "--puzzle",
          71);

  const std::string clientId =
      client::ClientIdentity::loadOrCreate();

  if (clientId.empty()) {
    std::cerr
        << "Unable to create local client identity\n";

    return 1;
  }

  std::cout
      << "OpenPuzzle\n"
      << "----------\n"
      << "Server............. "
      << server
      << '\n'
      << "Client ID.......... "
      << clientId
      << '\n'
      << "Puzzle............. "
      << puzzleNumber
      << '\n'
      << "Requesting range...\n\n";

  client::HttpRangeClient httpClient(
      server);

  const auto assignment =
      httpClient.claim(
          clientId,
          puzzleNumber);

  if (!assignment) {
    std::cerr
        << "Unable to claim range: "
        << httpClient.lastError()
        << '\n';

    return 1;
  }

  printAssignment(*assignment);

  if (subcommand == "claim") {
    return 0;
  }

  const bool dryRun =
      hasArgument(
          args,
          "--dry-run");

  CommandContext context;

  if (!context.initialize()) {
    std::cerr
        << context.lastError()
        << '\n';

    return 1;
  }

  const std::string engine =
      getArgument(
          args,
          "--engine",
          "bitcrack");

  const std::string backend =
      getArgument(
          args,
          "--backend",
          "cuda");

  if (engine != "bitcrack") {
    throw std::runtime_error(
        "Only BitCrack is currently enabled");
  }

  if (backend != "cuda" &&
      backend != "opencl") {
    throw std::runtime_error(
        "Unsupported BitCrack backend: " +
        backend);
  }

  int device =
      getIntegerArgument(
          args,
          "--device",
          context.gpu);

  int blocks =
      getIntegerArgument(
          args,
          "--blocks",
          getIntegerArgument(
              args,
              "--b",
              256));

  int threads =
      getIntegerArgument(
          args,
          "--threads",
          getIntegerArgument(
              args,
              "--t",
              256));

  int points =
      getIntegerArgument(
          args,
          "--points",
          getIntegerArgument(
              args,
              "--p",
              256));

  const bool manualProfile =
      hasArgument(args, "--blocks") ||
      hasArgument(args, "--b") ||
      hasArgument(args, "--threads") ||
      hasArgument(args, "--t") ||
      hasArgument(args, "--points") ||
      hasArgument(args, "--p");

  const auto gpu =
      GpuManager::currentGpu();

  if (!manualProfile) {
    GpuProfileManager profiles(
        context.db);

    const auto profile =
        profiles.chooseBest(
            gpu.name,
            backend == "opencl"
                ? "OpenCL"
                : "CUDA",
            "BitCrack");

    if (profile) {
      blocks =
          profile->blocks;

      threads =
          profile->threads;

      points =
          profile->points;

      std::cout
          << "\nGPU profile found\n";
    } else {
      std::cout
          << "\nNo GPU profile available; "
          << "using defaults\n";
    }
  }

  std::string executable;

  if (backend == "cuda") {
    if (!context.bitcrack) {
      throw std::runtime_error(
          "BitCrack CUDA executable not configured");
    }

    executable =
        *context.bitcrack;
  } else {
    const auto opencl =
        ToolManager::bitcrackOpenCLPath();

    if (!opencl) {
      throw std::runtime_error(
          "BitCrack OpenCL executable not configured");
    }

    executable =
        *opencl;
  }

  PuzzleRecord puzzle;

  puzzle.id =
      assignment->puzzle;

  puzzle.number =
      assignment->puzzle;

  puzzle.name =
      "Puzzle " +
      std::to_string(
          assignment->puzzle);

  puzzle.address =
      assignment->target;

  puzzle.rangeStart =
      assignment->start;

  puzzle.rangeEnd =
      assignment->end;

  RangeRecord range;

  range.id =
      assignment->rangeId;

  range.puzzleId =
      puzzle.id;

  range.startKey =
      assignment->start;

  range.endKey =
      assignment->end;

  range.status =
      RangeStatus::Running;

  JobRecord job;

  job.id =
      assignment->rangeId;

  job.puzzleId =
      puzzle.id;

  job.rangeId =
      range.id;

  job.state =
      JobState::Running;

  WorkerEngineCapability capability;

  capability.engine =
      "BitCrack";

  capability.backend =
      backend == "opencl"
          ? "OpenCL"
          : "CUDA";

  capability.device =
      device;

  capability.blocks =
      blocks;

  capability.threads =
      threads;

  capability.points =
      points;

  capability.available =
      true;

  const auto workspace =
      assignmentWorkspace(
          assignment->assignmentId);

  std::filesystem::create_directories(
      workspace);

  EngineManager engineManager;

  ExecutionRequestBuilder builder(
      engineManager);

  auto request =
      builder.build(
          puzzle,
          range,
          job,
          capability,
          executable,
          workspace.string());

  request.executionId =
      assignment->rangeId;

  std::cout
      << "\nLocal configuration\n"
      << "-------------------\n"
      << "GPU................ "
      << gpu.name
      << '\n'
      << "Engine............. "
      << capability.engine
      << '\n'
      << "Backend............ "
      << capability.backend
      << '\n'
      << "Executable......... "
      << executable
      << '\n'
      << "Device............. "
      << device
      << '\n'
      << "Blocks............. "
      << blocks
      << '\n'
      << "Threads............ "
      << threads
      << '\n'
      << "Points............. "
      << points
      << '\n'
      << "Workspace.......... "
      << request.workspace
      << "\n\n"
      << "Command\n"
      << "-------\n"
      << request.command
      << "\n\n";

  if (dryRun) {
    std::cout
        << "Dry run only. BitCrack was not started.\n";

    return 0;
  }

  BackgroundExecutionLauncher launcher;

  const auto handle =
      launcher.start(
          request);

  client::ClientExecutionState state;

  state.active = true;
  state.assignmentId =
      assignment->assignmentId;
  state.clientId =
      clientId;

  state.puzzle =
      assignment->puzzle;
  state.rangeId =
      assignment->rangeId;
  state.pid =
      handle.pid;

  state.target =
      assignment->target;
  state.start =
      assignment->start;
  state.end =
      assignment->end;

  state.engine =
      capability.engine;
  state.backend =
      capability.backend;

  state.workspace =
      handle.workspace;
  state.command =
      request.command;

  if (!client::ClientStateStore::save(
          state)) {
    ExecutionStopper stopper;

    stopper.stop(
        handle.workspace);

    throw std::runtime_error(
        "Unable to save local execution state");
  }

  std::cout
      << "BitCrack started.\n"
      << "PID................ "
      << handle.pid
      << '\n'
      << "\nUse:\n"
      << "  OpenPuzzle status\n"
      << "  OpenPuzzle stop\n";

  return 0;
}

} // namespace openpuzzle
