#pragma once

#include <string>

namespace openpuzzle {

struct BitCrackConfiguration {
  std::string cudaPath;
  std::string openclPath;
};

struct GpuConfiguration {
  int device = 0;
};

struct Configuration {
  BitCrackConfiguration bitcrack;
  GpuConfiguration gpu;
};

} // namespace openpuzzle
