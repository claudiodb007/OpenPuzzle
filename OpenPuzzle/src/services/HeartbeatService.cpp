#include "openpuzzle/services/HeartbeatService.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/models/Models.hpp"

namespace openpuzzle {

HeartbeatService::HeartbeatService(Database& database)
    : database_(database) {}

void HeartbeatService::update(
    const std::string& machine,
    const std::string& gpu,
    const std::string& backend,
    const std::string& engine,
    const std::string& status,
    double speed,
    double temperature,
    double power) {
  WorkerRecord worker;

  worker.machine = machine;
  worker.gpuName = gpu;
  worker.backend = backend;
  worker.engine = engine;
  worker.status = status;
  worker.speedMkeys = speed;
  worker.temperature = temperature;
  worker.power = power;

  database_.upsertWorker(worker);
}

bool HeartbeatService::update(
    int workerId,
    const std::string& status,
    double speed,
    double temperature,
    double power) {
  return database_.updateWorkerHeartbeat(
      workerId,
      status,
      speed,
      temperature,
      power);
}

int HeartbeatService::expireStale(int timeoutSeconds) {
  return database_.markStaleWorkersOffline(timeoutSeconds);
}

} // namespace openpuzzle
