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

struct ClientHeartbeat {
  std::string clientId;
  std::string version;
  std::string platform;
  std::string status = "idle";

  ClientCpuCapability cpu;
  std::vector<ClientGpuCapability> gpus;

  bool valid() const {
    return !clientId.empty() &&
           !version.empty() &&
           !platform.empty() &&
           (
             status == "idle" ||
             status == "running" ||
             status == "paused"
           ) &&
           cpu.valid();
  }
};

} // namespace openpuzzle::client
