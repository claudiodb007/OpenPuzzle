#include "openpuzzle/runtime/ExecutionSlot.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using openpuzzle::ExecutionSlot;

int main() {
  assert(ExecutionSlot::valid("primary"));
  assert(ExecutionSlot::valid("gpu"));
  assert(ExecutionSlot::valid("cpu"));
  assert(ExecutionSlot::valid("cuda"));
  assert(ExecutionSlot::valid("opencl"));

  assert(ExecutionSlot::cuda(0) == "cuda-0");
  assert(ExecutionSlot::cuda(1) == "cuda-1");
  assert(ExecutionSlot::cuda(32) == "cuda-32");
  assert(ExecutionSlot::opencl(0) == "opencl-0");
  assert(ExecutionSlot::opencl(32) == "opencl-32");
  assert(
      ExecutionSlot::cuda(
          std::numeric_limits<std::uint64_t>::max()) ==
      "cuda-18446744073709551615");

  assert(ExecutionSlot::valid("cuda-0"));
  assert(ExecutionSlot::valid("cuda-1"));
  assert(ExecutionSlot::valid("cuda-32"));
  assert(ExecutionSlot::valid("opencl-0"));
  assert(ExecutionSlot::valid("opencl-32"));
  assert(
      ExecutionSlot::valid(
          "cuda-18446744073709551615"));

  assert(ExecutionSlot::cudaDevice("cuda-0") == 0);
  assert(ExecutionSlot::cudaDevice("cuda-32") == 32);
  assert(ExecutionSlot::openclDevice("opencl-0") == 0);
  assert(ExecutionSlot::openclDevice("opencl-32") == 32);
  assert(
      ExecutionSlot::cudaDevice(
          "cuda-18446744073709551615") ==
      std::numeric_limits<std::uint64_t>::max());

  assert(!ExecutionSlot::valid(""));
  assert(!ExecutionSlot::valid("cuda-"));
  assert(!ExecutionSlot::valid("cuda--1"));
  assert(!ExecutionSlot::valid("cuda-01"));
  assert(!ExecutionSlot::valid("cuda-1/../../escape"));
  assert(!ExecutionSlot::valid("cuda-18446744073709551616"));
  assert(!ExecutionSlot::valid("CUDA-1"));
  assert(!ExecutionSlot::valid("opencl-"));
  assert(!ExecutionSlot::valid("opencl-01"));
  assert(!ExecutionSlot::valid("OPENCL-1"));

  assert(ExecutionSlot::fileSuffix("primary").empty());
  assert(ExecutionSlot::fileSuffix("cuda") == "-cuda");
  assert(ExecutionSlot::fileSuffix("opencl") == "-opencl");
  assert(ExecutionSlot::fileSuffix("cuda-0") == "-cuda-0");
  assert(ExecutionSlot::fileSuffix("cuda-999") == "-cuda-999");
  assert(ExecutionSlot::fileSuffix("opencl-999") == "-opencl-999");
  assert(ExecutionSlot::fileSuffix("../../escape").empty());

  const auto directory =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-execution-slot-discovery-" +
          std::to_string(getpid()));

  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);

  for (const auto& filename : {
           "client-cuda-5.state",
           "runtime-cuda-0.pid",
           "safestop-cuda-2.requested",
           "client-cuda-2.state",
           "client-cuda-01.state",
           "runtime-opencl.pid",
           "runtime-opencl-1.pid",
           "client-opencl-3.state",
           "unrelated.txt",
       }) {
    std::ofstream output(
        directory / filename);
    assert(output);
  }

  assert(
      ExecutionSlot::discoverCudaSlots(
          directory) ==
      std::vector<std::string>({
          "cuda-0",
          "cuda-2",
          "cuda-5",
      }));

  assert(
      ExecutionSlot::discoverGpuSlots(
          directory) ==
      std::vector<std::string>({
          "cuda-0",
          "cuda-2",
          "cuda-5",
          "opencl-1",
          "opencl-3",
      }));

  std::filesystem::remove_all(directory);

  std::cout << "ExecutionSlotTests passed\n";
  return 0;
}
