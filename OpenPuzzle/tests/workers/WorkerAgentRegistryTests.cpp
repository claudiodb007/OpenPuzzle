#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

static WorkerAgent makeAgent(
    int id,
    const char* machine,
    const char* engine,
    const char* backend) {
  WorkerAgentInfo info;
  info.workerId = id;
  info.machine = machine;
  info.engine = engine;
  info.backend = backend;
  info.state = WorkerAgentState::Idle;

  return WorkerAgent(info);
}

int main() {
  WorkerAgentRegistry registry;

  assert(registry.count() == 0);

  assert(registry.add(makeAgent(1, "cuda-node", "BitCrack", "CUDA")));
  assert(registry.add(makeAgent(2, "opencl-node", "BitCrack", "OpenCL")));
  assert(registry.add(makeAgent(3, "keyhunt-node", "KeyHunt", "CPU")));

  assert(!registry.add(makeAgent(1, "duplicate", "BitCrack", "CUDA")));
  assert(!registry.add(makeAgent(0, "invalid", "BitCrack", "CUDA")));

  assert(registry.count() == 3);

  auto* first = registry.find(1);
  assert(first);
  assert(first->info().machine == "cuda-node");

  auto* cuda = registry.acquireIdle("BitCrack", "CUDA");
  assert(cuda);
  assert(cuda->info().workerId == 1);

  cuda->markBusy();

  auto* nextIdle = registry.acquireIdle("BitCrack", "CUDA");
  assert(nextIdle == nullptr);

  auto* opencl = registry.acquireIdle("BitCrack", "OpenCL");
  assert(opencl);
  assert(opencl->info().workerId == 2);

  assert(registry.all().size() == 3);

  registry.clear();
  assert(registry.count() == 0);

  std::cout << "WorkerAgentRegistryTests passed\n";
  return 0;
}
