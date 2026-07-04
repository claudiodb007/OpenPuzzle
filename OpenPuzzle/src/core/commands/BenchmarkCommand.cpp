#include "openpuzzle/core/commands/BenchmarkCommand.hpp"

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/BenchmarkRunner.hpp"
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

static std::string dbPath() {
  const char *home = std::getenv("HOME");

  if (!home)
    throw std::runtime_error("HOME not set");

  return std::string(home) + "/.local/share/OpenPuzzle/openpuzzle.db";
}

static bool openDb(Database &db) {
  return db.open(dbPath()) && db.createSchema();
}

int BenchmarkCommand::run(const std::vector<std::string> &args) const {
  int gpu = getIntArg(args, "--gpu",
                      getIntArg(args, "--d", GpuManager::selectedGpu()));

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

  auto bitcrack = ToolManager::bitcrackPath();

  if (!bitcrack)
    throw std::runtime_error("Engine not configured");

  Database db;
  if (!openDb(db))
    return 1;

  auto puzzle = db.getPuzzleByNumber(getIntArg(args, "--puzzle", 71));
  auto job = db.getJob(getIntArg(args, "--job", 1));

  if (!puzzle || !job)
    throw std::runtime_error("Puzzle/job not found");

  auto range = db.getRange(job->rangeId);

  if (!range)
    throw std::runtime_error("Range not found");

  Scheduler scheduler;

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
  ctx.engine = "Benchmark";
  ctx.workspace = "";
  ctx.command = command;
  ctx.echoOutput = true;

  BenchmarkRunner runner;
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
