#include "openpuzzle/engines/EngineLaunchBuilder.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

int main() {
  PuzzleRecord puzzle;
  puzzle.id = 71;
  puzzle.number = 71;
  puzzle.address =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

  RangeRecord range;
  range.id = 8;
  range.puzzleId = 71;
  range.startKey =
      "400000070000000000";
  range.endKey =
      "40000007FFFFFFFFFF";

  JobRecord job;
  job.id = 8;
  job.puzzleId = 71;
  job.rangeId = 8;

  WorkerEngineCapability capability;
  capability.engine = "BitCrack";
  capability.backend = "CUDA";
  capability.device = 1;
  capability.blocks = 512;
  capability.threads = 1024;
  capability.points = 4096;

  const auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-launch-builder-" +
       std::to_string(getpid())) /
      "jobs" /
      "000008";

  std::filesystem::create_directories(
      workspace);

  EngineLaunchBuilder builder;

  auto request = builder.build(
      puzzle,
      range,
      job,
      capability,
      workspace.string());

  assert(request.engine == "BitCrack");
  assert(request.backend == "CUDA");

  assert(request.targets.size() == 1);
  assert(
      request.targets.front() ==
      puzzle.address);

  assert(
      request.startKey ==
      range.startKey);

  assert(
      request.endKey ==
      range.endKey);

  assert(request.device == 1);
  assert(request.blocks == 512);
  assert(request.threads == 1024);
  assert(request.points == 4096);

  assert(
      request.targetFile.find(
          "targets.txt") !=
      std::string::npos);

  assert(
      std::filesystem::exists(
          request.targetFile));

  assert(
      request.outputFile.find(
          "found.txt") !=
      std::string::npos);

  assert(
      request.logFile.find(
          "bitcrack.log") !=
      std::string::npos);

  assert(
      request.workspace ==
      workspace.string());

  std::filesystem::remove_all(
      workspace.parent_path()
          .parent_path());

  std::cout
      << "EngineLaunchBuilderTests passed\n";

  return 0;
}
