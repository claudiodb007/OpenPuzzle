#include "openpuzzle/performance/AutoTuner.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <tuple>
#include <vector>

namespace openpuzzle {

namespace {

bool sameConfiguration(
    const BenchmarkConfiguration &left,
    const BenchmarkConfiguration &right) {
  return
      left.blocks == right.blocks &&
      left.threads == right.threads &&
      left.points == right.points;
}

void addUnique(
    std::vector<BenchmarkConfiguration> &result,
    const BenchmarkConfiguration &candidate,
    int memoryMb) {
  if (
      candidate.blocks <= 0 ||
      candidate.threads <= 0 ||
      candidate.points <= 0) {
    return;
  }

  /*
   * Keep a safety margin for the display server,
   * driver allocations and backend overhead.
   */
  if (memoryMb > 0) {
    const int budgetMb =
        memoryMb * 70 / 100;

    if (
        AutoTuner::estimatedMemoryMb(
            candidate) >
        budgetMb) {
      return;
    }
  }

  const auto existing =
      std::find_if(
          result.begin(),
          result.end(),
          [&](const auto &item) {
            return sameConfiguration(
                item,
                candidate);
          });

  if (existing ==
      result.end()) {
    result.push_back(candidate);
  }
}

} // namespace

int AutoTuner::estimatedMemoryMb(
    const BenchmarkConfiguration &configuration) {
  /*
   * BitCrack uses approximately 40 bytes for each
   * starting point, but that allocation is only
   * part of the total device-memory requirement.
   *
   * Real measurements include working buffers,
   * backend allocations and fixed driver overhead.
   * A conservative portable estimate is:
   *
   *   1024 MiB fixed overhead
   *   + 3.2 × starting-point memory
   *
   * The matrix still keeps only 70% of reported
   * VRAM available to benchmark configurations.
   */
  const std::int64_t startingBytes =
      static_cast<std::int64_t>(
          configuration.blocks) *
      configuration.threads *
      configuration.points *
      40;

  const std::int64_t mib =
      1024LL *
      1024LL;

  const std::int64_t startingMb =
      (startingBytes + mib - 1) /
      mib;

  const std::int64_t workingMb =
      (
          startingMb * 16 +
          4
      ) /
      5;

  return static_cast<int>(
      1024 + workingMb);
}

std::vector<BenchmarkResult>
AutoTuner::defaultMatrix(
    int computeUnits,
    int memoryMb) const {
  std::vector<int> blockValues;

  if (computeUnits > 0) {
    for (const int multiplier :
         {2, 4, 8}) {
      blockValues.push_back(
          computeUnits *
          multiplier);
    }
  } else {
    /*
     * Backend-neutral conservative fallback.
     * Configuration failures are safely ignored
     * during tuning.
     */
    blockValues = {
        32,
        64,
        128,
        256
    };
  }

  std::vector<BenchmarkConfiguration>
      configurations;

  /*
   * First cover block scaling using BitCrack's
   * conventional 256-thread launch.
   */
  for (const int blocks :
       blockValues) {
    for (const int points :
         {512, 1024}) {
      addUnique(
          configurations,
          {
              blocks,
              256,
              points
          },
          memoryMb);
    }
  }

  /*
   * Explore thread occupancy around the central
   * block count without creating a huge Cartesian
   * product.
   */
  const int centralBlocks =
      blockValues[
          blockValues.size() / 2];

  /*
   * 128 threads explores lower occupancy safely.
   *
   * 512-thread launches remain available through
   * explicit command-line options, but are omitted
   * from the portable automatic matrix because
   * several BitCrack backends reject them or need
   * disproportionately large working buffers.
   */
  for (const int threads :
       {128}) {
    for (const int points :
         {512, 1024}) {
      addUnique(
          configurations,
          {
              centralBlocks,
              threads,
              points
          },
          memoryMb);
    }
  }

  if (configurations.empty()) {
    addUnique(
        configurations,
        {
            computeUnits > 0
                ? computeUnits
                : 32,
            256,
            256
        },
        0);
  }

  std::vector<BenchmarkResult> result;

  result.reserve(
      configurations.size());

  for (const auto &configuration :
       configurations) {
    BenchmarkResult candidate;
    candidate.configuration =
        configuration;

    result.push_back(candidate);
  }

  return result;
}

BenchmarkResult
AutoTuner::selectBest(
    const std::vector<BenchmarkResult> &results) const {
  BenchmarkResult best;
  double bestScore = 0.0;

  for (const auto &result : results) {
    if (!result.success) {
      continue;
    }

    const double average =
        result.averageSpeed > 0.0
            ? result.averageSpeed
            : result.speedMKeys;

    if (average <= 0.0) {
      continue;
    }

    const double spread =
        result.maximumSpeed >
                result.minimumSpeed &&
            result.minimumSpeed > 0.0
            ? result.maximumSpeed -
                  result.minimumSpeed
            : 0.0;

    /*
     * Penalize unstable configurations so a short
     * peak cannot beat sustained throughput.
     */
    const double score =
        average -
        spread * 0.25;

    if (
        !best.success ||
        score > bestScore ||
        (
            score == bestScore &&
            average >
                best.averageSpeed
        )) {
      best = result;
      bestScore = score;
    }
  }

  return best;
}

double AutoTuner::planningSpeed(
    const BenchmarkResult &result,
    double factor) {
  const double measured =
      result.averageSpeed > 0.0
          ? result.averageSpeed
          : result.speedMKeys;

  if (measured <= 0.0) {
    return 0.0;
  }

  if (
      factor <= 0.0 ||
      factor > 1.0) {
    factor = 1.0;
  }

  return measured * factor;
}

std::string AutoTuner::buildCommand(
    const std::string &bitcrackPath,
    const PuzzleRecord &puzzle,
    const RangeRecord &range,
    int gpu,
    int blocks,
    int threads,
    int points,
    const std::string &outputFile) const {
  std::ostringstream command;

  command
      << bitcrackPath
      << " "
      << puzzle.address
      << " --keyspace "
      << range.startKey
      << ":"
      << range.endKey
      << " --out "
      << outputFile
      << " -d "
      << gpu
      << " -b "
      << blocks
      << " -t "
      << threads
      << " -p "
      << points;

  return command.str();
}

} // namespace openpuzzle
