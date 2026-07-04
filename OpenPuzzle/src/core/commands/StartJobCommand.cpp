#include "openpuzzle/core/commands/StartJobCommand.hpp"

#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace openpuzzle {

static bool hasArg(const std::vector<std::string> &args,
                   const std::string &name) {
  for (const auto &arg : args)
    if (arg == name)
      return true;
  return false;
}

static int getIntArg(const std::vector<std::string> &args,
                     const std::string &name, int fallback) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i)
    if (args[i] == name)
      return std::stoi(args[i + 1]);

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

int StartJobCommand::run(const std::vector<std::string> &args) const {

  int puzzle = getIntArg(args, "--puzzle", 71);

  int job = getIntArg(args, "--job", 0);

  int blocks = getIntArg(args, "--blocks", getIntArg(args, "--b", 256));

  int threads = getIntArg(args, "--threads", getIntArg(args, "--t", 256));

  int points = getIntArg(args, "--points", getIntArg(args, "--p", 256));

  int device = getIntArg(args, "--device",
                         getIntArg(args, "--d", GpuManager::selectedGpu()));

  bool dryRun = hasArg(args, "--dry-run");

  std::cout << "Puzzle............ " << puzzle << "\n";
  std::cout << "Job............... " << job << "\n";
  std::cout << "Device............ " << device << "\n";
  std::cout << "Blocks............ " << blocks << "\n";
  std::cout << "Threads........... " << threads << "\n";
  std::cout << "Points............ " << points << "\n";
  std::cout << "Dry run........... " << (dryRun ? "yes" : "no") << "\n";

  Database db;

  if (!openDb(db))
    return 1;

  const bool manual = hasArg(args, "--blocks") || hasArg(args, "--b") ||
                      hasArg(args, "--threads") || hasArg(args, "--t") ||
                      hasArg(args, "--points") || hasArg(args, "--p");

  if (!manual) {

    auto gpu = GpuManager::currentGpu();

    GpuProfileManager profiles(db);

    auto profile = profiles.chooseBest(gpu.name, "CUDA", "BitCrack");

    if (profile) {
      blocks = profile->blocks;
      threads = profile->threads;
      points = profile->points;

      std::cout << "\nGPU profile found\n";
      std::cout << "Using " << blocks << "/" << threads << "/" << points
                << "\n";
    } else {
      std::cout << "\nNo GPU profile available\n";
      std::cout
          << "Tip............... run: OpenPuzzle benchmark --real --auto --gpu "
          << device << "\n";
    }
  }

  auto bitcrack = ToolManager::bitcrackPath();

  if (!bitcrack)
    throw std::runtime_error("BitCrack not found");

  Scheduler scheduler;

  auto result = scheduler.startJob(db, puzzle, job, *bitcrack, device, blocks,
                                   threads, points, dryRun);

  std::cout << "\nJob................ " << result.jobId << "\n";
  std::cout << "Range.............. " << result.rangeId << "\n";
  std::cout << "Execution ID....... " << result.executionId << "\n";
  std::cout << "Exit code.......... " << result.exitCode << "\n";

  if (dryRun)
    std::cout << "Dry run only.\n";

  return result.exitCode;
}

} // namespace openpuzzle
