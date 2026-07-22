#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace openpuzzle::client {

struct ClientCpuCapability {
  std::string name;
  int cores = 0;
  int threads = 0;

  bool valid() const {
    return !name.empty() &&
           cores > 0 &&
           threads > 0;
  }
};

struct ClientGpuCapability {
  std::string backend;
  std::string name;
  std::uint64_t memoryMB = 0;

  bool valid() const {
    return !backend.empty() &&
           !name.empty();
  }
};

struct ClientEngineCapability {
  std::string name;
  std::string backend;

  bool installed = false;
  bool available = false;

  bool valid() const {
    return !name.empty() &&
           !backend.empty();
  }
};

struct ClientHeartbeat {
  std::string clientId;
  std::string version;
  std::string platform;
  std::string status = "idle";

  std::string activeEngine;
  std::string activeBackend;

  ClientCpuCapability cpu;
  std::vector<ClientGpuCapability> gpus;
  std::vector<ClientEngineCapability> engines;

  bool valid() const {
    return !clientId.empty() &&
           !version.empty() &&
           !platform.empty() &&
           (
             status == "idle" ||
             status == "running" ||
             status == "paused"
           ) &&
           (
             status != "running" ||
             (
               !activeEngine.empty() &&
               !activeBackend.empty()
             )
           ) &&
           cpu.valid();
  }
};

} // namespace openpuzzle::client
