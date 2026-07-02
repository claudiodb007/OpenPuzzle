#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/performance/BenchmarkConfiguration.hpp"
#include "openpuzzle/performance/BenchmarkRunner.hpp"

#include <filesystem>

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
  ctx.engine = "Benchmark";
  ctx.workspace = temp.string();
  ctx.command = "printf 'NVIDIA GeForce R 7918 / 11873MB | 1 target 1345.81 "
                "MKey/s (104,958,263,296 total) [00:01:18]\\n'";
  ctx.echoOutput = false;

  BenchmarkRunner runner;
  auto result = runner.run(cfg, ctx, 5);

  if (!result.success)
    return 1;
  if (result.speedMKeys != 1345.81)
    return 2;
  if (result.configuration.blocks != 256)
    return 3;
  if (result.configuration.points != 1024)
    return 4;

  std::filesystem::remove_all(temp);

  return 0;
}
