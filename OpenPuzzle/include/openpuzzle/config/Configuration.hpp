#pragma once

#include <string>

namespace openpuzzle {

struct BitCrackConfiguration {
  std::string cudaPath;
  std::string openclPath;
};

struct EngineConfiguration {
  std::string id;
  std::string backend;
  std::string executable;
};

struct GpuConfiguration {
  int device = 0;
};

struct AssignmentConfiguration {
  int durationMinutes = 60;
};

struct Configuration {
  EngineConfiguration engine;
  BitCrackConfiguration bitcrack;
  GpuConfiguration gpu;
  AssignmentConfiguration assignment;
};

} // namespace openpuzzle
