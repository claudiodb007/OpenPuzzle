#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

static WorkerAgent makeAgent(
    int workerId,
    const char* machine,
    std::initializer_list<WorkerEngineCapability> capabilities) {
  WorkerAgentInfo info;
  info.workerId = workerId;
  info.machine = machine;
  info.state = WorkerAgentState::Idle;
  info.capabilities = capabilities;

  return WorkerAgent(info);
}

int main() {
  WorkerEngineCapability cudaBitCrack;
  cudaBitCrack.engine = "BitCrack";
  cudaBitCrack.backend = "CUDA";
  cudaBitCrack.device = 0;
  cudaBitCrack.vramMb = 12288;
  cudaBitCrack.benchmarkSpeedMkeys = 1350.0;

  WorkerEngineCapability openclBitCrack;
  openclBitCrack.engine = "BitCrack";
  openclBitCrack.backend = "OpenCL";
  openclBitCrack.device = 0;
  openclBitCrack.vramMb = 8192;
  openclBitCrack.benchmarkSpeedMkeys = 400.0;

  WorkerEngineCapability fasterCudaBitCrack;
  fasterCudaBitCrack.engine = "BitCrack";
  fasterCudaBitCrack.backend = "CUDA";
  fasterCudaBitCrack.device = 1;
  fasterCudaBitCrack.vramMb = 16384;
  fasterCudaBitCrack.benchmarkSpeedMkeys = 2100.0;

  WorkerEngineCapability keyHuntCpu;
  keyHuntCpu.engine = "KeyHunt";
  keyHuntCpu.backend = "CPU";
  keyHuntCpu.benchmarkSpeedMkeys = 55.0;

  WorkerAgentRegistry registry;

  assert(registry.add(makeAgent(
      1,
      "cuda-node",
      {cudaBitCrack, keyHuntCpu})));

  assert(registry.add(makeAgent(
      2,
      "opencl-node",
      {openclBitCrack})));

  assert(registry.add(makeAgent(
      3,
      "fast-cuda-node",
      {fasterCudaBitCrack})));

  auto* cuda =
      registry.acquireIdle("BitCrack", "CUDA");

  assert(cuda);
  assert(cuda->info().workerId == 3);
  assert(cuda->supports("BitCrack", "CUDA"));
  assert(!cuda->supports("BitCrack", "OpenCL"));

  const auto* capability =
      cuda->bestCapability("BitCrack", "CUDA");

  assert(capability);
  assert(capability->device == 1);
  assert(capability->benchmarkSpeedMkeys == 2100.0);

  auto* opencl =
      registry.acquireIdle("BitCrack", "OpenCL");

  assert(opencl);
  assert(opencl->info().workerId == 2);

  auto* keyhunt =
      registry.acquireIdle("KeyHunt", "CPU");

  assert(keyhunt);
  assert(keyhunt->info().workerId == 1);

  auto* unavailable =
      registry.acquireIdle("Kangaroo", "CUDA");

  assert(unavailable == nullptr);

  cuda->markBusy();

  auto* fallback =
      registry.acquireIdle("BitCrack", "CUDA");

  assert(fallback);
  assert(fallback->info().workerId == 1);

  std::cout << "WorkerEngineCapabilityTests passed\n";
  return 0;
}
