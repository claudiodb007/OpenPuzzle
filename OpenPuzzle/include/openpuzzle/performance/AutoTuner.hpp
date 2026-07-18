#pragma once

#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/performance/BenchmarkResult.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

class AutoTuner {
public:
  /*
   * Build a portable matrix using the capabilities
   * reported by the selected BitCrack backend.
   */
  std::vector<BenchmarkResult>
  defaultMatrix(
      int computeUnits = 0,
      int memoryMb = 0) const;

  BenchmarkResult selectBest(
      const std::vector<BenchmarkResult> &results) const;

  static int estimatedMemoryMb(
      const BenchmarkConfiguration &configuration);

  /*
   * Assignment planning uses a small conservative
   * margin because short benchmarks can run above
   * long-term thermally sustained throughput.
   */
  static double planningSpeed(
      const BenchmarkResult &result,
      double factor = 0.97);

  std::string buildCommand(
      const std::string &bitcrackPath,
      const PuzzleRecord &puzzle,
      const RangeRecord &range,
      int gpu,
      int blocks,
      int threads,
      int points,
      const std::string &outputFile) const;
};

} // namespace openpuzzle
