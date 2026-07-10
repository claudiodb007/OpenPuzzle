#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>

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

  auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-worker-agent-" + std::to_string(getpid()));

  std::filesystem::remove_all(workspace);
  std::filesystem::create_directories(workspace);

  StartExecutionRequest request;
  request.executionId = 1;
  request.workspace = workspace.string();
  request.command = "exit 0";

  BackgroundExecutionLauncher launcher;

  auto handle = agent.execute(launcher, request);

  assert(agent.busy());
  assert(handle.executionId == 1);
  assert(handle.pid > 0);
  assert(handle.workspace == workspace.string());

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  assert(std::filesystem::exists(workspace / "process.pid"));
  assert(std::filesystem::exists(workspace / "exit.code"));

  agent.markIdle();
  assert(agent.idle());

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

  std::filesystem::remove_all(workspace);

  std::cout << "WorkerAgentTests passed\n";
  return 0;
}
