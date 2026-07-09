#include "openpuzzle/workers/WorkerAgent.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

int main() {
  WorkerAgentInfo info;
  info.machine = "test-machine";
  info.gpuName = "RTX 4070 Super";
  info.backend = "CUDA";
  info.engine = "BitCrack";

  WorkerAgent agent(info);

  assert(agent.online());
  assert(agent.idle());
  assert(!agent.busy());
  assert(!agent.offline());

  agent.markBusy();

  assert(agent.busy());
  assert(!agent.idle());
  assert(agent.state() == WorkerState::Busy);

  agent.markIdle();

  assert(agent.idle());
  assert(agent.state() == WorkerState::Idle);

  agent.markOffline();

  assert(agent.offline());
  assert(!agent.online());

  auto record = agent.toRecord();

  assert(record.machine == "test-machine");
  assert(record.gpuName == "RTX 4070 Super");
  assert(record.backend == "CUDA");
  assert(record.engine == "BitCrack");
  assert(record.status == "offline");

  assert(WorkerAgent::stateToString(WorkerState::Idle) == "idle");
  assert(WorkerAgent::stateToString(WorkerState::Busy) == "running");
  assert(WorkerAgent::stateToString(WorkerState::Offline) == "offline");

  assert(WorkerAgent::stateFromString("idle") == WorkerState::Idle);
  assert(WorkerAgent::stateFromString("running") == WorkerState::Busy);
  assert(WorkerAgent::stateFromString("busy") == WorkerState::Busy);
  assert(WorkerAgent::stateFromString("unknown") == WorkerState::Offline);

  std::cout << "WorkerAgentTests passed\n";
  return 0;
}
