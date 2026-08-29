#include "openpuzzle/engines/EngineManager.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace openpuzzle;

int main() {
  // Force the initial discovery case to be independent of any real
  // user-local PSCKangaroo installation. An explicitly configured invalid
  // absolute path must fail closed and must not fall back to HOME/XDG paths.
  assert(
      setenv(
          "OPENPUZZLE_KANGAROO_PATH",
          "/nonexistent/openpuzzle-test-psckangaroo",
          1) == 0);

  EngineManager manager;

  assert(manager.registry().find("bitcrack"));
  assert(manager.registry().find("BITCRACK"));
  assert(manager.registry().find("keyhunt"));
  assert(manager.registry().find("kangaroo"));

  const auto *kangarooDescriptor =
      manager.registry().find("kangaroo");
  assert(kangarooDescriptor);
  assert(kangarooDescriptor->capabilities.cuda);
  assert(kangarooDescriptor->capabilities.supportsKangarooSearch);
  assert(kangarooDescriptor->capabilities.requiresPublicKey);
  assert(!kangarooDescriptor->capabilities.supportsLinearSearch);
  assert(!kangarooDescriptor->runtime.installed);

  assert(
      !manager.resolveExecutable(
          "Kangaroo",
          "CUDA"));

  assert(
      setenv(
          "OPENPUZZLE_KANGAROO_PATH",
          "/bin/true",
          1) == 0);

  EngineManager externalManager;

  const auto *externalDescriptor =
      externalManager.registry().find("kangaroo");

  assert(externalDescriptor);
  assert(externalDescriptor->runtime.installed);

  const auto externalExecutable =
      externalManager.resolveExecutable(
          "KANGAROO",
          "CUDA");

  assert(externalExecutable);
  assert(*externalExecutable == "/bin/true");

  assert(
      !externalManager.resolveExecutable(
          "Kangaroo",
          "OpenCL"));

  assert(
      unsetenv(
          "OPENPUZZLE_KANGAROO_PATH") == 0);

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

  auto kangaroo =
      manager.create(
          "KANGAROO",
          "/tmp/psckangaroo");
  assert(kangaroo);
  assert(kangaroo->info().name == "Pollard Kangaroo");
  assert(kangaroo->info().backend == "CUDA");

  std::cout
      << "EngineManagerTests passed\n";

  return 0;
}
