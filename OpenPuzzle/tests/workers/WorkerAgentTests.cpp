#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
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
  assert(!agent.hasExecution());

  auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-worker-agent-" + std::to_string(getpid()));

  std::filesystem::remove_all(workspace);
  std::filesystem::create_directories(workspace);

  StartExecutionRequest request;
  request.executionId = 1;
  request.workspace = workspace.string();
  request.command = "sleep 9999";

  BackgroundExecutionLauncher launcher;
  ExecutionStopper stopper;

  auto handle = agent.execute(launcher, request);

  assert(agent.busy());
  assert(agent.hasExecution());
  assert(agent.currentExecution());
  assert(agent.currentExecution()->executionId == 1);
  assert(handle.pid > 0);

  bool duplicateRejected = false;

  try {
    agent.execute(launcher, request);
  } catch (const std::runtime_error&) {
    duplicateRejected = true;
  }

  assert(duplicateRejected);

  assert(agent.stop(stopper));
  assert(agent.idle());
  assert(!agent.hasExecution());

  assert(!agent.stop(stopper));

  agent.markOffline();

  bool offlineRejected = false;

  try {
    agent.execute(launcher, request);
  } catch (const std::runtime_error&) {
    offlineRejected = true;
  }

  assert(offlineRejected);

  auto record = agent.toRecord();

  assert(record.machine == "test-machine");
  assert(record.gpuName == "RTX 4070 Super");
  assert(record.backend == "CUDA");
  assert(record.engine == "BitCrack");
  assert(record.status == "offline");

  assert(WorkerAgent::stateToString(WorkerAgentState::Idle) == "idle");
  assert(WorkerAgent::stateToString(WorkerAgentState::Busy) == "running");
  assert(WorkerAgent::stateToString(WorkerAgentState::Offline) == "offline");

  assert(WorkerAgent::stateFromString("idle") == WorkerAgentState::Idle);
  assert(WorkerAgent::stateFromString("running") == WorkerAgentState::Busy);
  assert(WorkerAgent::stateFromString("busy") == WorkerAgentState::Busy);
  assert(WorkerAgent::stateFromString("unknown") == WorkerAgentState::Offline);

  std::filesystem::remove_all(workspace);

  std::cout << "WorkerAgentTests passed\n";
  return 0;
}
