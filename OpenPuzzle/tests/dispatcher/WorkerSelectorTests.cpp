#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/dispatcher/WorkerSelector.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

static WorkerRecord makeWorker(
    const char* machine,
    const char* gpu,
    const char* backend,
    const char* engine,
    const char* status,
    double speed) {
  WorkerRecord worker;
  worker.machine = machine;
  worker.gpuName = gpu;
  worker.backend = backend;
  worker.engine = engine;
  worker.status = status;
  worker.speedMkeys = speed;
  return worker;
}

int main() {
  Database db;

  assert(db.open(":memory:"));
  assert(db.createSchema());

  int slowId = db.upsertWorker(makeWorker(
      "slow-node",
      "RX 5700 XT",
      "OpenCL",
      "BitCrack",
      "idle",
      400.0));

  int fastId = db.upsertWorker(makeWorker(
      "fast-node",
      "RTX 4070 Super",
      "CUDA",
      "BitCrack",
      "idle",
      1350.0));

  int busyId = db.upsertWorker(makeWorker(
      "busy-node",
      "RTX 5080",
      "CUDA",
      "BitCrack",
      "running",
      2500.0));

  int keyhuntId = db.upsertWorker(makeWorker(
      "cpu-node",
      "CPU",
      "CPU",
      "KeyHunt",
      "idle",
      50.0));

  assert(slowId > 0);
  assert(fastId > 0);
  assert(busyId > 0);
  assert(keyhuntId > 0);

  WorkerSelector selector(db);

  auto best = selector.selectIdleWorker();
  assert(best);
  assert(best->id == fastId);
  assert(best->speedMkeys == 1350.0);

  auto cuda =
      selector.selectIdleWorker("BitCrack", "CUDA");
  assert(cuda);
  assert(cuda->id == fastId);

  auto opencl =
      selector.selectIdleWorker("BitCrack", "OpenCL");
  assert(opencl);
  assert(opencl->id == slowId);

  auto keyhunt =
      selector.selectIdleWorker("KeyHunt", "CPU");
  assert(keyhunt);
  assert(keyhunt->id == keyhuntId);

  auto unavailable =
      selector.selectIdleWorker("Kangaroo", "CUDA");
  assert(!unavailable);

  assert(db.updateWorkerStatus(fastId, "running"));

  auto fallback = selector.selectIdleWorker();
  assert(fallback);
  assert(fallback->id == slowId);

  std::cout << "WorkerSelectorTests passed\n";
  return 0;
}
