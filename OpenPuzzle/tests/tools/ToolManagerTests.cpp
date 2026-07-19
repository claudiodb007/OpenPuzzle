#include "openpuzzle/tools/ToolManager.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

using namespace openpuzzle;

int main() {
  const auto backend =
      ToolManager::bundledBackend();

  assert(
      backend == "opencl" ||
      backend == "cuda");

  const auto bundled =
      ToolManager::bundledBitCrackPath();

  assert(bundled);
  assert(
      std::filesystem::is_regular_file(
          *bundled));

  std::string error;

  assert(
      ToolManager::validateBitCrackEngine(
          *bundled,
          backend,
          &error));

  assert(error.empty());

  assert(
      !ToolManager::validateBitCrackEngine(
          "/bin/true",
          backend,
          &error));

  assert(!error.empty());

  assert(
      !ToolManager::configureBitCrack(
          "/tmp/external-bitcrack"));

  if (backend == "opencl") {
    assert(ToolManager::bitcrackOpenCLPath());
    assert(!ToolManager::bitcrackCudaPath());
  } else {
    assert(ToolManager::bitcrackCudaPath());
    assert(!ToolManager::bitcrackOpenCLPath());
  }

  std::cout
      << "ToolManagerTests passed\n";

  return 0;
}
