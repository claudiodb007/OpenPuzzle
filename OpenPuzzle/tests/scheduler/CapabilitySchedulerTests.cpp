#include "openpuzzle/scheduler/CapabilityScheduler.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"
#include "openpuzzle/workers/WorkerAgentRegistry.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

static WorkerEngineCapability capability(
    const char* engine,
    const char* backend,
    int device,
    double speed) {
  WorkerEngineCapability result;

  result.engine = engine;
  result.backend = backend;
  result.device = device;
  result.blocks = 256;
  result.threads = 256;
  result.points = 1024;
  result.benchmarkSpeedMkeys = speed;
  result.available = true;

  return result;
}

static WorkerAgent makeWorker(
    int workerId,
    WorkerAgentState state,
    std::initializer_list<WorkerEngineCapability> capabilities) {
  WorkerAgentInfo info;

  info.workerId = workerId;
  info.machine =
      "worker-" +
      std::to_string(workerId);

  info.state = state;
  info.capabilities = capabilities;

  return WorkerAgent(info);
}

int main() {
  WorkerAgentRegistry workers;

  assert(workers.add(
      makeWorker(
          1,
          WorkerAgentState::Idle,
          {
              capability(
                  "BitCrack",
                  "CUDA",
                  0,
                  1350.0),
              capability(
                  "KeyHunt",
                  "CPU",
                  0,
                  55.0)
          })));

  assert(workers.add(
      makeWorker(
          2,
          WorkerAgentState::Idle,
          {
              capability(
                  "BitCrack",
                  "OpenCL",
                  0,
                  400.0)
          })));

  assert(workers.add(
      makeWorker(
          3,
          WorkerAgentState::Idle,
          {
              capability(
                  "BitCrack",
                  "CUDA",
                  1,
                  2100.0)
          })));

  assert(workers.add(
      makeWorker(
          4,
          WorkerAgentState::Busy,
          {
              capability(
                  "BitCrack",
                  "CUDA",
                  2,
                  5000.0)
          })));

  CapabilityScheduler scheduler(workers);

  auto cuda =
      scheduler.select(
          "BitCrack",
          "CUDA");

  assert(cuda);
  assert(cuda->valid);
  assert(cuda->workerId == 3);
  assert(cuda->engine == "BitCrack");
  assert(cuda->backend == "CUDA");
  assert(cuda->capability.device == 1);
  assert(cuda->expectedSpeedMkeys == 2100.0);

  auto opencl =
      scheduler.select(
          "BitCrack",
          "OpenCL");

  assert(opencl);
  assert(opencl->workerId == 2);
  assert(opencl->expectedSpeedMkeys == 400.0);

  auto keyhunt =
      scheduler.select(
          "KeyHunt",
          "CPU");

  assert(keyhunt);
  assert(keyhunt->workerId == 1);
  assert(keyhunt->expectedSpeedMkeys == 55.0);

  auto unavailable =
      scheduler.select(
          "Kangaroo",
          "CUDA");

  assert(!unavailable);

  std::cout
      << "CapabilitySchedulerTests passed\n";

  return 0;
}
