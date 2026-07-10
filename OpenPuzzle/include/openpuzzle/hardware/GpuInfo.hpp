#pragma once

#include <string>

namespace openpuzzle {

struct GpuInfo {
  int device = 0;

  std::string name;
  std::string backend;
  std::string uuid;
  std::string driverVersion;

  int memoryMb = 0;
  int computeUnits = 0;

  bool cuda = false;
  bool opencl = false;
};

} // namespace openpuzzle
