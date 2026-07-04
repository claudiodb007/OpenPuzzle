#pragma once

#include <string>

namespace openpuzzle {

struct GpuInfo {
  int id = 0;
  std::string name;
  std::string backend;
  int memoryMb = 0;
  int computeUnits = 0;
};

} // namespace openpuzzle
