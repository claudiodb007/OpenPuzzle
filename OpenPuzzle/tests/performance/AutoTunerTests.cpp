#include "openpuzzle/performance/AutoTuner.hpp"

#include <vector>

using namespace openpuzzle;

int main() {
  AutoTuner tuner;

  /*
   * Dynamic matrix for a 56-CU GPU.
   */
  const auto matrix =
      tuner.defaultMatrix(
          56,
          12288);

  if (matrix.empty())
    return 1;

  bool foundRecommended = false;

  for (const auto &candidate :
       matrix) {
    const auto &configuration =
        candidate.configuration;

    if (
        configuration.blocks %
            56 !=
        0) {
      return 2;
    }

    if (
        configuration.threads %
            32 !=
        0) {
      return 3;
    }

    if (
        AutoTuner::estimatedMemoryMb(
            configuration) >
        12288 * 70 / 100) {
      return 4;
    }

    if (
        configuration.blocks == 224 &&
        configuration.threads == 256 &&
        configuration.points == 1024) {
      foundRecommended = true;
    }
  }

  if (!foundRecommended)
    return 5;

  /*
   * The estimate must cover fixed backend overhead
   * and working buffers observed during real runs.
   */
  const BenchmarkConfiguration validatedProfile{
      224,
      128,
      1024
  };

  if (
      AutoTuner::estimatedMemoryMb(
          validatedProfile) !=
      4608) {
    return 15;
  }

  const BenchmarkConfiguration conventionalProfile{
      224,
      256,
      1024
  };

  if (
      AutoTuner::estimatedMemoryMb(
          conventionalProfile) !=
      8192) {
    return 16;
  }

  const BenchmarkConfiguration excessiveProfile{
      448,
      256,
      1024
  };

  if (
      AutoTuner::estimatedMemoryMb(
          excessiveProfile) !=
      15360) {
    return 17;
  }

  for (const auto &candidate :
       matrix) {
    /*
     * 512-thread configurations may still be run
     * manually but are not portable auto choices.
     */
    if (
        candidate.configuration.threads >
        256) {
      return 18;
    }

    if (
        candidate.configuration.blocks == 448 &&
        candidate.configuration.threads == 256 &&
        candidate.configuration.points == 1024) {
      return 19;
    }
  }

  /*
   * Small-memory devices must not receive launch
   * configurations exceeding their budget.
   */
  const auto smallMemory =
      tuner.defaultMatrix(
          20,
          2048);

  if (smallMemory.empty())
    return 6;

  for (const auto &candidate :
       smallMemory) {
    if (
        AutoTuner::estimatedMemoryMb(
            candidate.configuration) >
        2048 * 70 / 100) {
      return 7;
    }
  }

  /*
   * Unknown compute units use a safe fallback.
   */
  const auto fallback =
      tuner.defaultMatrix(
          0,
          4096);

  if (fallback.empty())
    return 8;

  /*
   * Stability-aware selection.
   */
  std::vector<BenchmarkResult> results;

  BenchmarkResult fastButUnstable;
  fastButUnstable.configuration =
      {224, 256, 1024};
  fastButUnstable.speedMKeys = 1400.0;
  fastButUnstable.averageSpeed = 1400.0;
  fastButUnstable.minimumSpeed = 1000.0;
  fastButUnstable.maximumSpeed = 1600.0;
  fastButUnstable.samples = 8;
  fastButUnstable.success = true;

  BenchmarkResult stable;
  stable.configuration =
      {224, 512, 512};
  stable.speedMKeys = 1350.0;
  stable.averageSpeed = 1350.0;
  stable.minimumSpeed = 1320.0;
  stable.maximumSpeed = 1380.0;
  stable.samples = 8;
  stable.success = true;

  results.push_back(
      fastButUnstable);

  results.push_back(
      stable);

  const auto best =
      tuner.selectBest(results);

  if (!best.success)
    return 9;

  if (
      best.configuration.threads !=
      512) {
    return 10;
  }

  const double plannedSpeed =
      AutoTuner::planningSpeed(
          stable);

  if (plannedSpeed != 1309.5)
    return 11;

  PuzzleRecord puzzle;
  puzzle.address =
      "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH";

  RangeRecord range;
  range.startKey = "2";
  range.endKey =
      "FFFFFFFFFFFFFFFFFF";

  const auto command =
      tuner.buildCommand(
          "/opt/BitCrack/cuBitCrack",
          puzzle,
          range,
          0,
          224,
          256,
          1024,
          "/tmp/benchmark-found.txt");

  if (
      command.find("-b 224") ==
      std::string::npos) {
    return 12;
  }

  if (
      command.find("-t 256") ==
      std::string::npos) {
    return 13;
  }

  if (
      command.find("-p 1024") ==
      std::string::npos) {
    return 14;
  }

  return 0;
}
