#pragma once

#include "openpuzzle/client/ClientHeartbeat.hpp"
#include "openpuzzle/hardware/GpuInfo.hpp"
#include "openpuzzle/hardware/GpuTelemetry.hpp"

#include <string>

namespace openpuzzle::client {

struct ClientHeartbeatResult {
  bool success = false;
  ClientHeartbeat heartbeat;
  std::string error;
};

class ClientHeartbeatService {
public:
  static constexpr const char* TelemetryOwnerEnvironment =
      "OPENPUZZLE_HEARTBEAT_TELEMETRY_OWNER";

  ClientHeartbeatResult send(
      const std::string& serverUrl) const;

  static ClientHeartbeat collectLocalHeartbeat();

  static std::vector<ClientGpuCapability>
  gpuCapabilities(
      const std::vector<openpuzzle::GpuInfo>& inventory);

  static std::vector<ClientGpuTelemetry>
  gpuTelemetry(
      const std::vector<openpuzzle::GpuTelemetrySnapshot>& snapshots);

private:
  static bool shouldCollectTelemetry();

  static std::string platform();
  static ClientCpuCapability cpu();

  static std::vector<ClientGpuCapability>
  gpus();

  static std::vector<ClientGpuTelemetry>
  telemetry();

  static std::vector<ClientEngineCapability>
  engines();

  static std::string executionStatus(
      std::string& activeEngine,
      std::string& activeBackend,
      std::vector<std::string>& activeBackends,
      int& activeCpuThreads);
};

} // namespace openpuzzle::client
