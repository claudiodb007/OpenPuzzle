#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"
#include "openpuzzle/engines/EngineManager.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

static PuzzleRecord makePuzzle() {
  PuzzleRecord puzzle;
  puzzle.id = 71;
  puzzle.number = 71;
  puzzle.name = "Puzzle 71";
  puzzle.address =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
  puzzle.rangeStart =
      "400000000000000000";
  puzzle.rangeEnd =
      "7FFFFFFFFFFFFFFFFF";

  return puzzle;
}

static RangeRecord makeRange() {
  RangeRecord range;
  range.id = 8;
  range.puzzleId = 71;
  range.startKey =
      "400000070000000000";
  range.endKey =
      "40000007FFFFFFFFFF";
  range.blockBits = 40;

  return range;
}

static JobRecord makeJob() {
  JobRecord job;
  job.id = 8;
  job.puzzleId = 71;
  job.rangeId = 8;

  return job;
}

int main() {
  const auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-request-builder-" +
       std::to_string(getpid())) /
      "jobs" /
      "000008";

  std::filesystem::create_directories(
      workspace);

  WorkerEngineCapability capability;
  capability.engine = "BitCrack";
  capability.backend = "CUDA";
  capability.device = 1;
  capability.blocks = 512;
  capability.threads = 1024;
  capability.points = 4096;

  EngineManager engineManager;
  ExecutionRequestBuilder builder(engineManager);

  auto request = builder.build(
      makePuzzle(),
      makeRange(),
      makeJob(),
      capability,
      "/tmp/cuBitCrack",
      workspace.string());

  assert(request.engine == "BitCrack");
  assert(request.backend == "CUDA");

  assert(request.device == 1);
  assert(request.blocks == 512);
  assert(request.threads == 1024);
  assert(request.points == 4096);

  assert(
      request.command.find(
          "/tmp/cuBitCrack") !=
      std::string::npos);

  assert(
      request.command.find(
          "-d 1") !=
      std::string::npos);

  assert(
      request.command.find(
          "-b 512") !=
      std::string::npos);

  assert(
      request.command.find(
          "-t 1024") !=
      std::string::npos);

  assert(
      request.command.find(
          "-p 4096") !=
      std::string::npos);

  assert(
      request.command.find(
          "bitcrack.log") !=
      std::string::npos);

  std::filesystem::remove_all(
      workspace.parent_path()
          .parent_path());

  std::cout
      << "ExecutionRequestBuilderTests passed\n";

  return 0;
}
