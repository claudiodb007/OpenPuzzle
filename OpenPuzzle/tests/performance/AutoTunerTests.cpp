#include "openpuzzle/performance/AutoTuner.hpp"

using namespace openpuzzle;

int main() {
  AutoTuner tuner;

  auto matrix = tuner.defaultMatrix();

  if (matrix.size() != 12)
    return 1;
  if (matrix[0].blocks != 128)
    return 2;
  if (matrix[7].blocks != 256)
    return 3;
  if (matrix[7].threads != 256)
    return 4;
  if (matrix[7].points != 1024)
    return 5;

  std::vector<BenchmarkResult> results = {{128, 256, 256, 1200.0, true},
                                          {256, 256, 1024, 1345.81, true},
                                          {512, 512, 1024, 1300.0, true}};

  auto best = tuner.selectBest(results);

  if (!best.success)
    return 6;
  if (best.blocks != 256)
    return 7;
  if (best.points != 1024)
    return 8;
  if (best.speedMKeys != 1345.81)
    return 9;

  PuzzleRecord puzzle;
  puzzle.address = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

  RangeRecord range;
  range.startKey = "400000000000000000";
  range.endKey = "40000000FFFFFFFFFF";

  auto command =
      tuner.buildCommand("/home/claudiodb/BitCrack/bin/cuBitCrack", puzzle,
                         range, 0, 256, 256, 1024, "/tmp/found.txt");

  if (command.find("-d 0") == std::string::npos)
    return 10;
  if (command.find("-b 256") == std::string::npos)
    return 11;
  if (command.find("-t 256") == std::string::npos)
    return 12;
  if (command.find("-p 1024") == std::string::npos)
    return 13;

  return 0;
}
