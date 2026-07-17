#include "openpuzzle/engines/common/EngineRegistry.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

static EngineDescriptor makeEngine(
    const char* id,
    const char* name,
    bool installed,
    bool available,
    bool cuda,
    bool opencl,
    bool cpu) {
  EngineDescriptor engine;

  engine.id = id;
  engine.name = name;
  engine.backend =
      cuda && opencl
          ? "CUDA/OpenCL"
          : cuda
                ? "CUDA"
                : opencl
                      ? "OpenCL"
                      : "CPU";

  engine.runtime.installed = installed;
  engine.runtime.available = available;

  engine.capabilities.cuda = cuda;
  engine.capabilities.opencl = opencl;
  engine.capabilities.cpu = cpu;

  return engine;
}

int main() {
  EngineRegistry registry;

  assert(registry.count() == 0);

  registry.registerEngine(
      makeEngine(
          "bitcrack",
          "BitCrack",
          true,
          true,
          true,
          true,
          false));

  registry.registerEngine(
      makeEngine(
          "keyhunt",
          "KeyHunt",
          false,
          false,
          false,
          false,
          true));

  assert(registry.count() == 2);

  assert(registry.find("bitcrack"));
  assert(registry.find("BITCRACK"));
  assert(registry.find("BitCrack"));
  assert(!registry.find("kangaroo"));

  assert(registry.installed().size() == 1);
  assert(registry.available().size() == 1);

  auto cuda =
      registry.supportingBackend(
          "CUDA");

  assert(cuda.size() == 1);
  assert(cuda.front().id == "bitcrack");

  auto opencl =
      registry.supportingBackend(
          "opencl");

  assert(opencl.size() == 1);
  assert(opencl.front().id == "bitcrack");

  auto cpu =
      registry.supportingBackend(
          "CPU");

  assert(cpu.size() == 1);
  assert(cpu.front().id == "keyhunt");

  auto installedCpu =
      registry.supportingBackend(
          "CPU",
          true);

  assert(installedCpu.empty());

  auto replacement =
      makeEngine(
          "BITCRACK",
          "BitCrack Updated",
          true,
          true,
          true,
          false,
          false);

  registry.registerEngine(replacement);

  assert(registry.count() == 2);

  const auto* updated =
      registry.find("bitcrack");

  assert(updated);
  assert(updated->name ==
         "BitCrack Updated");
  assert(updated->capabilities.cuda);
  assert(!updated->capabilities.opencl);

  registry.clear();

  assert(registry.count() == 0);
  assert(registry.engines().empty());

  std::cout
      << "EngineRegistryTests passed\n";

  return 0;
}
