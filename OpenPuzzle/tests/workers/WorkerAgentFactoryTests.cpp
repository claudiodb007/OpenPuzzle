#include "openpuzzle/workers/WorkerAgentFactory.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

int main() {
  ExecutionResource resource;

  resource.id = "CUDA:0";
  resource.name = "RTX 4070 Super";
  resource.engine = "BitCrack";
  resource.backend = "CUDA";
  resource.device = 0;
  resource.memoryMb = 12288;

  resource.capability.engine = "BitCrack";
  resource.capability.backend = "CUDA";
  resource.capability.device = 0;
  resource.capability.vramMb = 12288;
  resource.capability.blocks = 256;
  resource.capability.threads = 512;
  resource.capability.points = 2048;

  auto basic =
      WorkerAgentFactory::create(resource);

  assert(basic.info().machine == "RTX 4070 Super");
  assert(basic.info().gpuName == "RTX 4070 Super");
  assert(basic.info().engine == "BitCrack");
  assert(basic.info().backend == "CUDA");
  assert(basic.info().capabilities.size() == 1);

  WorkerAgentInfo persisted;
  persisted.workerId = 42;
  persisted.machine = "office-pc";
  persisted.gpuName = "RTX 4070 Super";
  persisted.engine = "BitCrack";
  persisted.backend = "CUDA";
  persisted.state = WorkerAgentState::Busy;
  persisted.speedMkeys = 1350.0;
  persisted.temperature = 61.0;
  persisted.power = 180.0;

  auto worker =
      WorkerAgentFactory::create(
          resource,
          persisted);

  assert(worker.info().workerId == 42);
  assert(worker.info().machine == "office-pc");
  assert(worker.info().gpuName == "RTX 4070 Super");
  assert(worker.info().state == WorkerAgentState::Busy);
  assert(worker.info().speedMkeys == 1350.0);
  assert(worker.info().temperature == 61.0);
  assert(worker.info().power == 180.0);

  const auto* capability =
      worker.bestCapability(
          "BitCrack",
          "CUDA");

  assert(capability);
  assert(capability->device == 0);
  assert(capability->blocks == 256);
  assert(capability->threads == 512);
  assert(capability->points == 2048);

  std::cout
      << "WorkerAgentFactoryTests passed\n";

  return 0;
}
