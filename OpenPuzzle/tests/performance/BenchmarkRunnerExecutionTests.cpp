#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/performance/BenchmarkConfiguration.hpp"
#include "openpuzzle/performance/BenchmarkRunner.hpp"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <csignal>
#include <unistd.h>

using namespace openpuzzle;

int main() {
  auto temp = std::filesystem::temp_directory_path() /
              "openpuzzle_benchmark_runner_execution_test";

  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);

  BenchmarkConfiguration cfg;
  cfg.blocks = 256;
  cfg.threads = 256;
  cfg.points = 1024;

  ExecutionContext ctx;
  ctx.executionId = 1;
  ctx.puzzleId = 71;
  ctx.jobId = 42;
  ctx.rangeId = 1001;
  ctx.engine = "BitCrack";
  ctx.workspace = temp.string();
  ctx.command =
      "printf '"
      "GPU | 900.00 MKey/s (1,000 total) [00:00:01]\\n"
      "GPU | 1000.00 MKey/s (2,000 total) [00:00:02]\\n"
      "GPU | 1300.00 MKey/s (3,000 total) [00:00:03]\\n"
      "GPU | 1320.00 MKey/s (4,000 total) [00:00:04]\\n"
      "GPU | 1280.00 MKey/s (5,000 total) [00:00:05]\\n"
      "'";
  ctx.echoOutput = false;

  BenchmarkRunner runner;
  auto result = runner.run(cfg, ctx, 5, 5);

  if (!result.success)
    return 1;
  if (result.speedMKeys != 1300.0)
    return 2;
  if (result.minimumSpeed != 1280.0)
    return 3;
  if (result.maximumSpeed != 1320.0)
    return 4;
  if (result.samples != 3)
    return 5;
  if (result.configuration.blocks != 256)
    return 6;
  if (result.configuration.points != 1024)
    return 7;

  // A cancellation after positive measurements must not become a valid result.
  bool cancelled = false;
  ctx.stopRequested = [&] { return cancelled; };
  ctx.onProgress = [&](const ExecutionResult& progress) {
    if (progress.speedSamples.size() >= 3) cancelled = true;
  };
  ctx.command += "; exec sleep 60";
  result = runner.run(cfg, ctx, 60, 0);
  if (!cancelled || result.success || result.samples == 0) return 8;

  // The predicate is also polled for a silent owned child, not only on output.
  ctx.onProgress = {};
  const auto before = std::chrono::steady_clock::now();
  ctx.stopRequested = [&] {
    return std::chrono::steady_clock::now() - before > std::chrono::milliseconds(300);
  };
  const auto pidFile = temp / "silent.pid";
  ctx.command = "echo $$ > '" + pidFile.string() + "'; exec sleep 60";
  result = runner.run(cfg, ctx, 60, 0);
  if (result.success || std::chrono::steady_clock::now() - before > std::chrono::seconds(3)) return 9;
  int pid = 0;
  std::ifstream(pidFile) >> pid;
  if (pid <= 0 || kill(pid, 0) == 0) return 10;

  // A request made before launch must not create a child at all.
  const auto unexpected = temp / "unexpected-child";
  ctx.stopRequested = [] { return true; };
  ctx.command = "touch '" + unexpected.string() + "'";
  if (runner.run(cfg, ctx, 60, 0).success || std::filesystem::exists(unexpected)) return 11;
  std::filesystem::remove_all(temp);
  return 0;
}
