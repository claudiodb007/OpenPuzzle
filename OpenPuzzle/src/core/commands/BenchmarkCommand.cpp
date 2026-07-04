#include "openpuzzle/core/commands/BenchmarkCommand.hpp"

#include "openpuzzle/hardware/GpuManager.hpp"

#include <iostream>
#include <string>

namespace openpuzzle {

static bool hasArg(const std::vector<std::string> &args,
                   const std::string &name) {
  for (const auto &arg : args) {
    if (arg == name)
      return true;
  }

  return false;
}

static int getIntArg(const std::vector<std::string> &args,
                     const std::string &name, int fallback) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == name)
      return std::stoi(args[i + 1]);
  }

  return fallback;
}

int BenchmarkCommand::run(const std::vector<std::string> &args) const {
  int gpu = getIntArg(args, "--gpu",
                      getIntArg(args, "--d", GpuManager::selectedGpu()));

  bool real = hasArg(args, "--real");
  bool autoMode = hasArg(args, "--auto");

  int seconds = getIntArg(args, "--seconds", 0);
  int samples = getIntArg(args, "--samples", 6);
  int blocks = getIntArg(args, "--blocks", getIntArg(args, "--b", 256));
  int threads = getIntArg(args, "--threads", getIntArg(args, "--t", 256));
  int points = getIntArg(args, "--points", getIntArg(args, "--p", 256));

  std::cout << "====================================\n";
  std::cout << "      OpenPuzzle GPU Benchmark\n";
  std::cout << "====================================\n\n";
  std::cout << "GPU............. " << gpu << "\n";
  std::cout << "Real............ " << (real ? "yes" : "no") << "\n";
  std::cout << "Auto............ " << (autoMode ? "yes" : "no") << "\n";
  std::cout << "Blocks.......... " << blocks << "\n";
  std::cout << "Threads......... " << threads << "\n";
  std::cout << "Points.......... " << points << "\n";
  std::cout << "Samples......... " << samples << "\n";
  std::cout << "Seconds......... " << seconds << "\n";

  return 0;
}

} // namespace openpuzzle
