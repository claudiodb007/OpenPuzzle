#pragma once

#include <string>

namespace openpuzzle {

struct PuzzleExecutionMetadata {
  int puzzle = 0;
  std::string searchMode = "linear";
  std::string address;
  std::string hash160;
  std::string keyspace;
  std::string publicKey;
  std::string requiredBackend;
};

struct PuzzleExecutionPlan {
  std::string engine;
  std::string backend;
  bool executable = false;
  std::string reason;
};

class PuzzleExecutionPlanner {
public:
  static PuzzleExecutionPlan plan(
      const PuzzleExecutionMetadata &metadata,
      bool cudaAvailable,
      bool openclAvailable,
      bool cpuAvailable,
      bool kangarooExecutorAvailable = false);
};

} // namespace openpuzzle
