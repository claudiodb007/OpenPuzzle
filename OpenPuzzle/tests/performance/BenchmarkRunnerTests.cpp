#include "openpuzzle/performance/BenchmarkRunner.hpp"

using namespace openpuzzle;

int main() {
  BenchmarkRunner runner;

  BenchmarkConfiguration cfg;
  cfg.blocks = 256;
  cfg.threads = 256;
  cfg.points = 1024;

  ExecutionContext ctx;

  auto result = runner.run(cfg, ctx);

  if (!result.success)
    return 1;

  if (result.configuration.points != 1024)
    return 2;

  return 0;
}
