#include "openpuzzle/engines/EngineManager.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

int main() {
  EngineManager manager;

  assert(manager.registry().find("bitcrack"));
  assert(manager.registry().find("BITCRACK"));
  assert(manager.registry().find("keyhunt"));

  assert(
      !manager.resolveExecutable(
          "unknown-engine",
          "CUDA"));

  assert(
      !manager.resolveExecutable(
          "BitCrack",
          "unsupported-backend"));

  auto engine =
      manager.create(
          "BITCRACK",
          "/tmp/cuBitCrack");

  assert(engine);
  assert(
      engine->info().name ==
      "BitCrack");

  std::cout
      << "EngineManagerTests passed\n";

  return 0;
}
