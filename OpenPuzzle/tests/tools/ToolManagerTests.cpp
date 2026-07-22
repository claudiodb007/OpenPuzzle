#include "openpuzzle/tools/ToolManager.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

using namespace openpuzzle;

int main() {
  assert(ToolManager::supportsBackend("cuda"));
  assert(ToolManager::supportsBackend("opencl"));
  assert(ToolManager::supportsBackend("cpu"));

  const auto backends =
      ToolManager::bundledBackends();

  assert(backends.size() == 3);
  assert(backends[0] == "cuda");
  assert(backends[1] == "opencl");
  assert(backends[2] == "cpu");

  const auto cuda =
      ToolManager::bitcrackCudaPath();

  const auto opencl =
      ToolManager::bitcrackOpenCLPath();

  assert(cuda);
  assert(opencl);

  assert(std::filesystem::is_regular_file(*cuda));
  assert(std::filesystem::is_regular_file(*opencl));

  std::string error;

  assert(
      ToolManager::validateBitCrackEngine(
          *cuda,
          "cuda",
          &error));
  assert(error.empty());

  assert(
      ToolManager::validateBitCrackEngine(
          *opencl,
          "opencl",
          &error));
  assert(error.empty());

  assert(
      !ToolManager::validateBitCrackEngine(
          *cuda,
          "opencl",
          &error));

  assert(
      !ToolManager::validateBitCrackEngine(
          "/bin/true",
          "cuda",
          &error));

  assert(!error.empty());

  assert(
      !ToolManager::configureBitCrack(
          "/tmp/external-bitcrack"));

  const auto preferred =
      ToolManager::preferredBackend();

  assert(
      preferred == "cuda" ||
      preferred == "opencl");

  const auto bundled =
      ToolManager::bundledBitCrackPath();

  assert(bundled);

  std::cout
      << "Preferred backend: "
      << preferred
      << '\n'
      << "ToolManagerTests passed\n";

  return 0;
}
