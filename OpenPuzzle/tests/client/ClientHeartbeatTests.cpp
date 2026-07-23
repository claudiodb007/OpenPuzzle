#include "openpuzzle/client/ClientHeartbeat.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle::client;

int main() {
  ClientHeartbeat heartbeat;

  assert(!heartbeat.valid());

  heartbeat.clientId =
      "11111111-1111-4111-8111-111111111111";

  heartbeat.version =
      "0.10.0";

  heartbeat.platform =
      "Ubuntu Linux";

  heartbeat.status =
      "idle";

  heartbeat.cpu.name =
      "Test CPU";

  heartbeat.cpu.cores = 8;
  heartbeat.cpu.threads = 16;

  assert(heartbeat.valid());

  heartbeat.status =
      "running";

  assert(!heartbeat.valid());

  heartbeat.activeEngine =
      "BitCrack";

  heartbeat.activeBackend =
      "CUDA";

  heartbeat.activeBackends = {
      "CUDA",
      "CPU",
  };

  assert(heartbeat.valid());

  heartbeat.status =
      "idle";

  heartbeat.activeEngine.clear();
  heartbeat.activeBackend.clear();

  assert(heartbeat.valid());

  ClientGpuCapability gpu;

  gpu.backend =
      "CUDA";

  gpu.name =
      "Test GPU";

  gpu.memoryMB =
      12288;

  assert(gpu.valid());

  heartbeat.gpus.push_back(gpu);

  ClientEngineCapability engine;

  engine.name =
      "BitCrack";

  engine.backend =
      "CUDA";

  engine.installed = true;
  engine.available = true;

  assert(engine.valid());

  heartbeat.engines.push_back(engine);

  assert(heartbeat.valid());

  heartbeat.status =
      "offline";

  assert(!heartbeat.valid());

  std::cout
      << "ClientHeartbeatTests passed\n";

  return 0;
}
