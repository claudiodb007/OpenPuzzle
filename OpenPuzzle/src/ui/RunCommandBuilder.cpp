#include "openpuzzle/ui/RunCommandBuilder.hpp"

#include <array>
#include <stdexcept>

namespace openpuzzle::ui {

bool RunCommandBuilder::supportsPuzzle(
    RunMode mode,
    int puzzle) {
  if (puzzle <= 0) {
    return false;
  }

  if (mode != RunMode::KangarooCuda) {
    return true;
  }

  constexpr std::array<int, 5> supported = {
      140,
      145,
      150,
      155,
      160,
  };

  for (const int candidate : supported) {
    if (candidate == puzzle) {
      return true;
    }
  }

  return false;
}

std::vector<std::string> RunCommandBuilder::build(
    const RunSelection& selection) {
  switch (selection.mode) {
  case RunMode::BitCrackCuda:
  case RunMode::BitCrackOpencl:
  case RunMode::BitCrackCudaOpencl:
  case RunMode::KeyHuntCpu:
  case RunMode::KangarooCuda:
    break;
  default:
    throw std::invalid_argument(
        "Unsupported execution mode");
  }

  if (!supportsPuzzle(selection.mode, selection.puzzle)) {
    throw std::invalid_argument(
        selection.mode == RunMode::KangarooCuda
            ? "Kangaroo supports puzzles 140, 145, 150, 155 and 160"
            : "Puzzle must be a positive number");
  }

  std::vector<std::string> arguments = {
      "run",
      std::to_string(selection.puzzle),
  };

  if (selection.mode == RunMode::KeyHuntCpu) {
    if (selection.cpuThreads <= 0) {
      throw std::invalid_argument(
          "CPU threads must be a positive number");
    }

    arguments.insert(
        arguments.end(),
        {
            "--engine", "keyhunt",
            "--backend", "cpu",
            "--threads", std::to_string(selection.cpuThreads),
        });
    return arguments;
  }

  if (selection.device < 0) {
    throw std::invalid_argument(
        "GPU device must be non-negative");
  }

  if (selection.mode == RunMode::KangarooCuda) {
    arguments.insert(
        arguments.end(),
        {
            "--engine", "kangaroo",
            "--backend", "cuda",
            "--device", std::to_string(selection.device),
        });
    return arguments;
  }

  const bool openclOnly =
      selection.mode == RunMode::BitCrackOpencl;
  const bool concurrent =
      selection.mode == RunMode::BitCrackCudaOpencl;

  arguments.insert(
      arguments.end(),
      {
          "--engine", "bitcrack",
          "--backend", openclOnly ? "opencl" : "cuda",
      });

  arguments.push_back("--device");
  arguments.push_back(
      std::to_string(selection.device));

  if (concurrent) {
    if (selection.openclDevice < 0) {
      throw std::invalid_argument(
          "OpenCL device must be non-negative");
    }

    arguments.push_back("--with-opencl");
    arguments.push_back("--opencl-device");
    arguments.push_back(
        std::to_string(selection.openclDevice));
  }

  if (
      (openclOnly || concurrent) &&
      selection.rusticlRadeonsi) {
    arguments.push_back("--rusticl-enable");
    arguments.push_back("radeonsi");
  }

  return arguments;
}

} // namespace openpuzzle::ui
