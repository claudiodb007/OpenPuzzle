#include "openpuzzle/core/commands/BenchmarkCommand.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/AutoTuner.hpp"
#include "openpuzzle/performance/BenchmarkRunner.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace openpuzzle {

static bool hasArg(const std::vector<std::string> &args,
                   const std::string &name) {
  for (const auto &arg : args) {
    if (arg == name)
      return true;
  }

  return false;
}

static int getIntArg(const std::vector<std::string> &args,
                     const std::string &name, int fallback) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == name)
      return std::stoi(args[i + 1]);
  }

  return fallback;
}

static std::string getStringArg(
    const std::vector<std::string> &args,
    const std::string &name,
    const std::string &fallback) {
  for (
      std::size_t index = 0;
      index + 1 < args.size();
      ++index) {
    if (args[index] == name) {
      return args[index + 1];
    }
  }

  return fallback;
}

static std::string normalizeBackend(
    std::string backend) {
  std::transform(
      backend.begin(),
      backend.end(),
      backend.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  return backend;
}

int BenchmarkCommand::run(const std::vector<std::string> &args) const {
  CommandContext context;

  if (!context.initialize()) {
    std::cerr << context.lastError() << "\n";
    return 1;
  }

  int gpu = getIntArg(
      args,
      "--gpu",
      getIntArg(
          args,
          "--d",
          context.gpu));

  const auto configuration =
      ConfigurationManager::load();

  const std::string configuredBackend =
      configuration.engine.backend.empty()
          ? "cuda"
          : configuration.engine.backend;

  const std::string backend =
      normalizeBackend(
          getStringArg(
              args,
              "--backend",
              configuredBackend));

  if (
      backend != "cuda" &&
      backend != "opencl") {
    throw std::runtime_error(
        "Unsupported BitCrack backend: " +
        backend);
  }

  if (backend !=
      ToolManager::bundledBackend()) {
    throw std::runtime_error(
        "This OpenPuzzle package only supports the " +
        ToolManager::bundledBackend() +
        " backend");
  }

  const std::string backendLabel =
      backend == "opencl"
          ? "OpenCL"
          : "CUDA";

  bool real = hasArg(args, "--real");
  bool autoMode = hasArg(args, "--auto");

  int seconds = getIntArg(args, "--seconds", 30);
  int samples = getIntArg(args, "--samples", 8);
  int blocks = getIntArg(args, "--blocks", getIntArg(args, "--b", 256));
  int threads = getIntArg(args, "--threads", getIntArg(args, "--t", 256));
  int points = getIntArg(args, "--points", getIntArg(args, "--p", 256));

  if (seconds < 10 || seconds > 300) {
    throw std::runtime_error(
        "Benchmark duration must be between 10 and 300 seconds");
  }

  if (samples < 4 || samples > 60) {
    throw std::runtime_error(
        "Benchmark samples must be between 4 and 60");
  }

  if (blocks <= 0 || threads <= 0 || points <= 0) {
    throw std::runtime_error(
        "Benchmark launch parameters must be positive");
  }

  std::cout << "====================================\n";
  std::cout << "      OpenPuzzle GPU Benchmark\n";
  std::cout << "====================================\n\n";
  const auto gpuInfo =
      GpuManager::currentGpu(
          backendLabel);

  std::cout << "Backend......... "
            << backendLabel
            << "\n";
  std::cout << "GPU............. " << gpu << "\n";
  std::cout << "GPU name........ " << gpuInfo.name << "\n";
  std::cout << "Compute units... " << gpuInfo.computeUnits << "\n";
  std::cout << "Memory.......... " << gpuInfo.memoryMb << " MiB\n";
  std::cout << "Maximum seconds. " << seconds << "\n";
  std::cout << "Target samples.. " << samples << "\n";
  std::cout << "Warm-up samples. 2\n\n";

  if (!real) {
    std::cout << "Use --real to run a real benchmark.\n";
    (void)autoMode;
    return 0;
  }

  const auto bitcrack =
      backend == "opencl"
          ? ToolManager::bitcrackOpenCLPath()
          : ToolManager::bitcrackCudaPath();

  if (!bitcrack) {
    throw std::runtime_error(
        "BitCrack " +
        backendLabel +
        " executable not configured");
  }

  if (
      !fs::is_regular_file(
          *bitcrack)) {
    throw std::runtime_error(
        "BitCrack " +
        backendLabel +
        " executable was not found: " +
        *bitcrack);
  }

  /*
   * A public client starts with an empty local
   * database. Benchmark execution must therefore
   * never depend on puzzle, job or range records.
   *
   * The synthetic target has private key 1 while
   * the benchmark starts at key 2. Consequently,
   * no private key can be produced.
   */
  Database &db = context.db;

  PuzzleRecord benchmarkPuzzle{};
  benchmarkPuzzle.address =
      "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH";

  RangeRecord benchmarkRange{};
  benchmarkRange.startKey = "2";

  /*
   * A large synthetic interval prevents BitCrack
   * from reaching the end during benchmarking.
   */
  benchmarkRange.endKey =
      "FFFFFFFFFFFFFFFFFF";

  Scheduler &scheduler = context.scheduler;

  const char *home =
      std::getenv("HOME");

  if (
      home == nullptr ||
      *home == '\0') {
    throw std::runtime_error(
        "HOME is not available for benchmark workspace");
  }

  const auto workspace =
      fs::path(home) /
      ".local" /
      "share" /
      "OpenPuzzle" /
      "benchmark";

  WorkspaceSecurity::prepare(
      workspace);

  const auto output =
      (workspace /
       "benchmark-safe-found.txt")
          .string();

  /*
   * This file belongs only to the synthetic target
   * above and can never contain a real puzzle key.
   */
  fs::remove(output);
  fs::remove(
      workspace /
      "stdout.log");

  BenchmarkConfiguration cfg;
  cfg.blocks = blocks;
  cfg.threads = threads;
  cfg.points = points;

  auto command = scheduler.buildBitCrackCommand(bitcrack.value(), benchmarkPuzzle, benchmarkRange, gpu,
                                                cfg.blocks, cfg.threads,
                                                cfg.points, output);

  ExecutionContext ctx;
  ctx.executionId = 0;
  ctx.puzzleId = 0;
  ctx.jobId = 0;
  ctx.rangeId = 0;
  ctx.engine = "BitCrack";
  ctx.workspace =
      workspace.string();
  ctx.command = command;
  /*
   * Never echo raw engine output. Benchmark output
   * is performance data only.
   */
  ctx.echoOutput = false;

  BenchmarkRunner runner;

  if (autoMode) {
    AutoTuner tuner;

    const auto candidates =
        tuner.defaultMatrix(
            gpuInfo.computeUnits,
            gpuInfo.memoryMb);

    std::vector<BenchmarkResult> results;

    std::cout
        << "Candidate matrix. "
        << candidates.size()
        << " configurations\n";

    int index = 1;

    for (const auto &candidate :
         candidates) {
      const auto &configuration =
          candidate.configuration;

      const auto configCommand =
          scheduler.buildBitCrackCommand(
              bitcrack.value(),
              benchmarkPuzzle,
              benchmarkRange,
              gpu,
              configuration.blocks,
              configuration.threads,
              configuration.points,
              output);

      ctx.command = configCommand;

      std::cout
          << "\n["
          << index
          << "/"
          << candidates.size()
          << "] "
          << "b="
          << configuration.blocks
          << " t="
          << configuration.threads
          << " p="
          << configuration.points
          << " memory~"
          << AutoTuner::estimatedMemoryMb(
                 configuration)
          << " MiB\n";

      const auto item =
          runner.run(
              configuration,
              ctx,
              seconds,
              samples);

      results.push_back(item);

      std::cout
          << "Average.......... "
          << item.averageSpeed
          << " MKey/s\n"
          << "Minimum.......... "
          << item.minimumSpeed
          << " MKey/s\n"
          << "Maximum.......... "
          << item.maximumSpeed
          << " MKey/s\n"
          << "Samples.......... "
          << item.samples
          << "\n"
          << "Success.......... "
          << (
                 item.success
                     ? "yes"
                     : "no")
          << "\n";

      ++index;
    }

    auto best =
        tuner.selectBest(results);

    if (!best.success) {
      std::cerr
          << "\nNo valid benchmark "
             "configuration was found.\n";

      return 1;
    }

    const int validationSeconds =
        std::min(
            300,
            std::max(
                60,
                seconds * 2));

    const int validationSamples =
        std::min(
            60,
            std::max(
                16,
                samples * 2));

    std::cout
        << "\nValidating finalist\n"
        << "-------------------\n"
        << "Blocks........... "
        << best.configuration.blocks
        << "\n"
        << "Threads.......... "
        << best.configuration.threads
        << "\n"
        << "Points........... "
        << best.configuration.points
        << "\n"
        << "Maximum seconds.. "
        << validationSeconds
        << "\n"
        << "Target samples... "
        << validationSamples
        << "\n";

    ctx.command =
        scheduler.buildBitCrackCommand(
            bitcrack.value(),
            benchmarkPuzzle,
            benchmarkRange,
            gpu,
            best.configuration.blocks,
            best.configuration.threads,
            best.configuration.points,
            output);

    const auto validated =
        runner.run(
            best.configuration,
            ctx,
            validationSeconds,
            validationSamples);

    if (!validated.success) {
      std::cerr
          << "Final validation.. failed\n"
          << "GPU profile was not changed.\n";

      return 1;
    }

    best = validated;

    const double planningSpeed =
        AutoTuner::planningSpeed(best);

    std::cout
        << "\nValidated configuration\n"
        << "-----------------------\n"
        << "Blocks........... "
        << best.configuration.blocks
        << "\n"
        << "Threads.......... "
        << best.configuration.threads
        << "\n"
        << "Points........... "
        << best.configuration.points
        << "\n"
        << "Average.......... "
        << best.averageSpeed
        << " MKey/s\n"
        << "Minimum.......... "
        << best.minimumSpeed
        << " MKey/s\n"
        << "Maximum.......... "
        << best.maximumSpeed
        << " MKey/s\n"
        << "Samples.......... "
        << best.samples
        << "\n"
        << "Planning speed... "
        << planningSpeed
        << " MKey/s\n";

    GpuProfileRecord profile;

    profile.gpuName =
        gpuInfo.name;
    profile.backend =
        backendLabel;
    profile.engine = "BitCrack";
    profile.blocks =
        best.configuration.blocks;
    profile.threads =
        best.configuration.threads;
    profile.points =
        best.configuration.points;
    profile.averageSpeed =
        planningSpeed;
    profile.minimumSpeed =
        best.minimumSpeed;
    profile.maximumSpeed =
        best.maximumSpeed;
    profile.samples =
        best.samples;

    GpuProfileManager profileManager(db);

    const bool saved =
        profileManager.save(profile);

    std::cout
        << "Saved profile..... "
        << (
               saved
                   ? "yes"
                   : "no")
        << "\n";

    return saved
               ? 0
               : 1;
  }

  auto result = runner.run(cfg, ctx, seconds, samples);

  std::cout << "\nBenchmark result\n\n";
  std::cout << "Blocks........... " << result.configuration.blocks << "\n";
  std::cout << "Threads.......... " << result.configuration.threads << "\n";
  std::cout << "Points........... " << result.configuration.points << "\n";
  std::cout << "Average.......... " << result.averageSpeed << " MKey/s\n";
  std::cout << "Minimum.......... " << result.minimumSpeed << " MKey/s\n";
  std::cout << "Maximum.......... " << result.maximumSpeed << " MKey/s\n";
  std::cout << "Samples.......... " << result.samples << "\n";
  std::cout << "Success.......... " << (result.success ? "yes" : "no") << "\n";

  return result.speedMKeys > 0.0 ? 0 : 1;
}

} // namespace openpuzzle
