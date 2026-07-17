#pragma once

#include "openpuzzle/client/ClientHeartbeat.hpp"

#include <string>

namespace openpuzzle::client {

struct ClientHeartbeatResult {
  bool success = false;
  ClientHeartbeat heartbeat;
  std::string error;
};

class ClientHeartbeatService {
public:
  ClientHeartbeatResult send(
      const std::string& serverUrl) const;

  static ClientHeartbeat collectLocalHeartbeat();

private:
  static std::string platform();
  static ClientCpuCapability cpu();

  static std::vector<ClientGpuCapability>
  gpus();

  static std::vector<ClientEngineCapability>
  engines();

  static std::string executionStatus();
};

} // namespace openpuzzle::client
