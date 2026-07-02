#include "openpuzzle/performance/AutoTuner.hpp"

#include <sstream>

namespace openpuzzle {

std::vector<BenchmarkResult> AutoTuner::defaultMatrix() const {
  return {
      {128, 256, 256, 0.0, false},  {256, 256, 256, 0.0, false},
      {512, 256, 256, 0.0, false},  {128, 256, 512, 0.0, false},
      {256, 256, 512, 0.0, false},  {512, 256, 512, 0.0, false},
      {128, 256, 1024, 0.0, false}, {256, 256, 1024, 0.0, false},
      {512, 256, 1024, 0.0, false}, {256, 512, 512, 0.0, false},
      {256, 512, 1024, 0.0, false}, {512, 512, 1024, 0.0, false},
  };
}

BenchmarkResult
AutoTuner::selectBest(const std::vector<BenchmarkResult> &results) const {
  BenchmarkResult best;

  for (const auto &result : results) {
    if (!result.success) {
      continue;
    }

    if (!best.success || result.speedMKeys > best.speedMKeys) {
      best = result;
    }
  }

  return best;
}

std::string AutoTuner::buildCommand(const std::string &bitcrackPath,
                                    const PuzzleRecord &puzzle,
                                    const RangeRecord &range, int gpu,
                                    int blocks, int threads, int points,
                                    const std::string &outputFile) const {
  std::ostringstream command;

  command << bitcrackPath << " " << puzzle.address << " --keyspace "
          << range.startKey << ":" << range.endKey << " --out " << outputFile
          << " -d " << gpu << " -b " << blocks << " -t " << threads << " -p "
          << points;

  return command.str();
}

} // namespace openpuzzle
