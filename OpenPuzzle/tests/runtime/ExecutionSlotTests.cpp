#include "openpuzzle/runtime/ExecutionSlot.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

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
  assert(
      ExecutionSlot::cuda(
          std::numeric_limits<std::uint64_t>::max()) ==
      "cuda-18446744073709551615");

  assert(ExecutionSlot::valid("cuda-0"));
  assert(ExecutionSlot::valid("cuda-1"));
  assert(ExecutionSlot::valid("cuda-32"));
  assert(
      ExecutionSlot::valid(
          "cuda-18446744073709551615"));

  assert(ExecutionSlot::cudaDevice("cuda-0") == 0);
  assert(ExecutionSlot::cudaDevice("cuda-32") == 32);
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

  assert(ExecutionSlot::fileSuffix("primary").empty());
  assert(ExecutionSlot::fileSuffix("cuda") == "-cuda");
  assert(ExecutionSlot::fileSuffix("opencl") == "-opencl");
  assert(ExecutionSlot::fileSuffix("cuda-0") == "-cuda-0");
  assert(ExecutionSlot::fileSuffix("cuda-999") == "-cuda-999");
  assert(ExecutionSlot::fileSuffix("../../escape").empty());

  std::cout << "ExecutionSlotTests passed\n";
  return 0;
}
