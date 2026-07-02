#pragma once

#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/performance/BenchmarkResult.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class AutoTuner {
public:
  std::vector<BenchmarkResult> defaultMatrix() const;

  BenchmarkResult selectBest(const std::vector<BenchmarkResult> &results) const;

  std::string buildCommand(const std::string &bitcrackPath,
                           const PuzzleRecord &puzzle, const RangeRecord &range,
                           int gpu, int blocks, int threads, int points,
                           const std::string &outputFile) const;
};

} // namespace openpuzzle
