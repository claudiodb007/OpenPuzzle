#pragma once

#include <cstdint>
#include <cmath>
#include <optional>
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

struct ClientGpuTelemetry {
  std::string vendor;
  std::string deviceId;
  std::string pciBusId;
  std::string temperatureLabel;

  int device = -1;

  std::optional<double> temperatureC;
  std::optional<double> powerDrawW;
  std::optional<double> powerLimitW;

  bool valid() const {
    const auto finite = [](const std::optional<double>& value) {
      return !value || std::isfinite(*value);
    };

    return !vendor.empty() &&
           !deviceId.empty() &&
           device >= 0 &&
           (
             temperatureC.has_value() ||
             powerDrawW.has_value() ||
             powerLimitW.has_value()
           ) &&
           finite(temperatureC) &&
           finite(powerDrawW) &&
           finite(powerLimitW) &&
           (!temperatureC ||
             (*temperatureC >= -100.0 && *temperatureC <= 300.0)) &&
           (!powerDrawW || *powerDrawW >= 0.0) &&
           (!powerLimitW || *powerLimitW >= 0.0);
  }
};

struct ClientHeartbeat {
  std::string clientId;
  std::string version;
  std::string platform;
  std::string status = "idle";

  std::string activeEngine;
  std::string activeBackend;
  std::vector<std::string> activeBackends;

  ClientCpuCapability cpu;
  std::vector<ClientGpuCapability> gpus;
  bool gpuTelemetryReported = false;
  std::vector<ClientGpuTelemetry> gpuTelemetry;
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
