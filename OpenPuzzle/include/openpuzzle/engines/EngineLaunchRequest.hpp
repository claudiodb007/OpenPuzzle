#pragma once

#include "openpuzzle/database/Database.hpp"

#include <string>

namespace openpuzzle {

struct EngineLaunchRequest {
  PuzzleRecord puzzle;
  RangeRecord range;

  int device = 0;
  int blocks = 256;
  int threads = 256;
  int points = 256;

  std::string workspace;
  std::string targetFile;
  std::string outputFile;
  std::string logFile;
};

} // namespace openpuzzle
