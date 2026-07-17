#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/runtime/LocalExecutionBackend.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerRuntime.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

using namespace openpuzzle;

static WorkerAgent makeWorker() {
  WorkerEngineCapability capability;
  capability.engine = "BitCrack";
  capability.backend = "CUDA";
  capability.device = 0;
  capability.blocks = 256;
  capability.threads = 256;
  capability.points = 1024;
  capability.available = true;

  WorkerAgentInfo info;
  info.workerId = 7;
  info.machine = "local-worker";
  info.gpuName = "RTX 4070 Super";
  info.engine = "BitCrack";
  info.backend = "CUDA";
  info.state = WorkerAgentState::Idle;
  info.capabilities.push_back(
      capability);

  return WorkerAgent(info);
}

int main() {
  auto worker = makeWorker();

  LocalExecutionBackend backend;

  WorkerRuntime runtime(
      worker,
      backend);

  assert(runtime.idle());
  assert(!runtime.busy());
  assert(!runtime.currentExecution());

  const auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-worker-runtime-" +
       std::to_string(getpid()));

  std::filesystem::remove_all(workspace);
  std::filesystem::create_directories(workspace);

  StartExecutionRequest invalid;
  assert(!runtime.start(invalid));

  StartExecutionRequest wrongEngine;
  wrongEngine.executionId = 1;
  wrongEngine.jobId = 1;
  wrongEngine.engine = "KeyHunt";
  wrongEngine.backend = "CPU";
  wrongEngine.workspace = workspace.string();
  wrongEngine.command = "exit 0";

  assert(!runtime.start(wrongEngine));

  StartExecutionRequest request;
  request.executionId = 12;
  request.puzzleId = 71;
  request.jobId = 42;
  request.rangeId = 1001;
  request.engine = "BitCrack";
  request.backend = "CUDA";
  request.workspace = workspace.string();
  request.command =
      "sleep 1; exit 0";

  assert(runtime.start(request));
  assert(runtime.busy());
  assert(!runtime.idle());
  assert(runtime.currentExecution());
  assert(
      runtime.currentExecution()
          ->executionId == 12);

  assert(!runtime.start(request));

  std::this_thread::sleep_for(
      std::chrono::milliseconds(100));

  assert(runtime.stop());
  assert(runtime.idle());
  assert(!runtime.busy());
  assert(!runtime.currentExecution());

  std::filesystem::remove_all(workspace);

  std::cout
      << "WorkerRuntimeTests passed\n";

  return 0;
}
