#pragma once

#include <string>
#include <vector>

namespace openpuzzle {

struct EngineLaunchRequest {
  std::string engine;
  std::string backend;

  std::vector<std::string> targets;

  std::string startKey;
  std::string endKey;

  int device = 0;
  int blocks = 0;
  int threads = 0;
  int points = 0;

  std::string workspace;
  std::string targetFile;
  std::string outputFile;
  std::string logFile;
};

} // namespace openpuzzle
