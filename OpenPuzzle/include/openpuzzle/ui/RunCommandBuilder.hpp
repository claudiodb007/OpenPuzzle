#pragma once

#include <string>
#include <vector>

namespace openpuzzle::ui {

enum class RunMode {
  BitCrackCuda,
  BitCrackOpencl,
  BitCrackCudaOpencl,
  KeyHuntCpu,
  KangarooCuda,
};

struct RunSelection {
  int puzzle = 71;
  RunMode mode = RunMode::BitCrackCuda;
  int device = 0;
  int openclDevice = 0;
  int cpuThreads = 1;
  bool rusticlRadeonsi = false;
};

class RunCommandBuilder {
public:
  static bool supportsPuzzle(
      RunMode mode,
      int puzzle);

  static std::vector<std::string> build(
      const RunSelection& selection);
};

} // namespace openpuzzle::ui
