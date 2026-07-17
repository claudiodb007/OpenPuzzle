#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/services/HeartbeatService.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace openpuzzle;

int main() {
  Database db;

  assert(db.open(":memory:"));
  assert(db.createSchema());

  WorkerRecord worker;
  worker.machine = "heartbeat-node";
  worker.gpuName = "RTX 4070 Super";
  worker.backend = "CUDA";
  worker.engine = "BitCrack";
  worker.status = "idle";
  worker.speedMkeys = 1350.0;
  worker.temperature = 60.0;
  worker.power = 180.0;

  int workerId = db.upsertWorker(worker);
  assert(workerId > 0);

  HeartbeatService heartbeat(db);

  assert(heartbeat.update(
      workerId,
      "running",
      1400.0,
      62.0,
      185.0));

  auto refreshed = db.getWorker(workerId);
  assert(refreshed);
  assert(refreshed->status == "running");
  assert(refreshed->speedMkeys == 1400.0);
  assert(refreshed->temperature == 62.0);
  assert(refreshed->power == 185.0);

  std::this_thread::sleep_for(
      std::chrono::milliseconds(2100));

  int expired = heartbeat.expireStale(1);
  assert(expired == 1);

  auto stale = db.getWorker(workerId);
  assert(stale);
  assert(stale->status == "offline");

  assert(heartbeat.update(
      workerId,
      "idle",
      1300.0,
      58.0,
      170.0));

  auto recovered = db.getWorker(workerId);
  assert(recovered);
  assert(recovered->status == "idle");
  assert(recovered->speedMkeys == 1300.0);

  std::cout << "WorkerHeartbeatTests passed\n";
  return 0;
}
