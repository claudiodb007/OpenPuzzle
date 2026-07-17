#include "openpuzzle/core/commands/BenchmarkCommand.hpp"

#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/AutoTuner.hpp"
#include "openpuzzle/performance/BenchmarkRunner.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

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

int BenchmarkCommand::run(const std::vector<std::string> &args) const {
  CommandContext context;

  if (!context.initialize()) {
    std::cerr << context.lastError() << "\n";
    return 1;
  }

  int gpu = getIntArg(args, "--gpu", getIntArg(args, "--d", context.gpu));

  bool real = hasArg(args, "--real");
  bool autoMode = hasArg(args, "--auto");

  int seconds = getIntArg(args, "--seconds", 0);
  int samples = getIntArg(args, "--samples", 6);
  int blocks = getIntArg(args, "--blocks", getIntArg(args, "--b", 256));
  int threads = getIntArg(args, "--threads", getIntArg(args, "--t", 256));
  int points = getIntArg(args, "--points", getIntArg(args, "--p", 256));

  std::cout << "====================================\n";
  std::cout << "      OpenPuzzle GPU Benchmark\n";
  std::cout << "====================================\n\n";
  std::cout << "GPU............. " << gpu << "\n\n";

  if (!real) {
    std::cout << "Use --real to run a real benchmark.\n";
    (void)autoMode;
    return 0;
  }

  auto bitcrack = context.bitcrack;

  if (!bitcrack)
    throw std::runtime_error("Engine not configured");

  Database &db = context.db;

  auto puzzle = db.getPuzzleByNumber(getIntArg(args, "--puzzle", 71));
  auto job = db.getJob(getIntArg(args, "--job", 1));

  if (!puzzle || !job)
    throw std::runtime_error("Puzzle/job not found");

  auto range = db.getRange(job->rangeId);

  if (!range)
    throw std::runtime_error("Range not found");

  Scheduler &scheduler = context.scheduler;

  auto workspace = scheduler.workspaceForJob(job->id);
  auto output = (fs::path(workspace) / "benchmark-found.txt").string();

  BenchmarkConfiguration cfg;
  cfg.blocks = blocks;
  cfg.threads = threads;
  cfg.points = points;

  auto command = scheduler.buildBitCrackCommand(*bitcrack, *puzzle, *range, gpu,
                                                cfg.blocks, cfg.threads,
                                                cfg.points, output);

  ExecutionContext ctx;
  ctx.executionId = 0;
  ctx.puzzleId = puzzle->id;
  ctx.jobId = job->id;
  ctx.rangeId = range->id;
  ctx.engine = "BitCrack";
  ctx.workspace = "";
  ctx.command = command;
  ctx.echoOutput = true;

  BenchmarkRunner runner;

  if (autoMode) {
    std::vector<BenchmarkConfiguration> configs;

    for (int threadsValue : {256, 512}) {
      for (int pointsValue : {512, 1024, 2048}) {
        BenchmarkConfiguration config;
        config.blocks = 256;
        config.threads = threadsValue;
        config.points = pointsValue;
        configs.push_back(config);
      }
    }

    std::vector<BenchmarkResult> results;

    int index = 1;
    for (const auto &config : configs) {
      auto configCommand = scheduler.buildBitCrackCommand(
          *bitcrack, *puzzle, *range, gpu, config.blocks, config.threads,
          config.points, output);

      ctx.command = configCommand;

      std::cout << "\n[" << index << "/" << configs.size() << "] ";
      std::cout << "b=" << config.blocks << " ";
      std::cout << "t=" << config.threads << " ";
      std::cout << "p=" << config.points << "\n";

      auto item = runner.run(config, ctx, seconds, samples);
      results.push_back(item);

      std::cout << "Average.......... " << item.averageSpeed << " MKey/s\n";
      std::cout << "Minimum.......... " << item.minimumSpeed << " MKey/s\n";
      std::cout << "Maximum.......... " << item.maximumSpeed << " MKey/s\n";
      std::cout << "Samples.......... " << item.samples << "\n";

      index++;
    }

    AutoTuner tuner;
    auto best = tuner.selectBest(results);

    std::cout << "\nBest configuration\n\n";
    std::cout << "Blocks........... " << best.configuration.blocks << "\n";
    std::cout << "Threads.......... " << best.configuration.threads << "\n";
    std::cout << "Points........... " << best.configuration.points << "\n";
    std::cout << "Average.......... " << best.averageSpeed << " MKey/s\n";

    if (best.success) {
      GpuProfileRecord profile;
      auto gpuInfo = GpuManager::currentGpu();

      profile.gpuName = gpuInfo.name;
      profile.backend = "CUDA";
      profile.engine = "BitCrack";
      profile.blocks = best.configuration.blocks;
      profile.threads = best.configuration.threads;
      profile.points = best.configuration.points;
      profile.averageSpeed = best.averageSpeed;
      profile.minimumSpeed = best.minimumSpeed;
      profile.maximumSpeed = best.maximumSpeed;
      profile.samples = best.samples;

      GpuProfileManager profileManager(db);
      const bool saved = profileManager.save(profile);

      std::cout << "Saved profile..... " << (saved ? "yes" : "no") << "\n";
    }

    return best.success ? 0 : 1;
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
