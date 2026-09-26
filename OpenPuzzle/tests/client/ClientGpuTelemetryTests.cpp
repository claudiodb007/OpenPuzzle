#include "openpuzzle/client/ClientHeartbeatService.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace openpuzzle;
using namespace openpuzzle::client;

int main() {
  GpuTelemetrySnapshot nvidia;
  nvidia.vendor = "NVIDIA";
  nvidia.deviceId = "cuda-0";
  nvidia.device = 0;
  nvidia.pciBusId = "00000000:01:00.0";
  nvidia.temperatureC = 64.0;
  nvidia.powerDrawW = 201.25;
  nvidia.powerLimitW = 245.0;

  GpuTelemetrySnapshot amd;
  amd.vendor = "AMD";
  amd.deviceId = "card2";
  amd.device = 2;
  amd.pciBusId = "0000:08:00.0";
  amd.temperatureLabel = "junction";
  amd.temperatureC = 52.0;
  amd.powerDrawW = 17.0;
  amd.powerLimitW = 135.0;

  GpuTelemetrySnapshot unavailable;
  unavailable.vendor = "AMD";
  unavailable.deviceId = "card3";
  unavailable.device = 3;

  const auto readings =
      ClientHeartbeatService::gpuTelemetry({
          nvidia,
          unavailable,
          amd,
      });

  assert(readings.size() == 2);

  assert(readings[0].valid());
  assert(readings[0].vendor == "NVIDIA");
  assert(readings[0].deviceId == "cuda-0");
  assert(readings[0].temperatureC == 64.0);
  assert(readings[0].powerDrawW == 201.25);

  assert(readings[1].valid());
  assert(readings[1].vendor == "AMD");
  assert(readings[1].deviceId == "card2");
  assert(readings[1].temperatureLabel == "junction");
  assert(readings[1].powerLimitW == 135.0);

  ClientGpuTelemetry invalid;
  invalid.vendor = "NVIDIA";
  invalid.deviceId = "cuda-1";
  invalid.device = 1;
  assert(!invalid.valid());

  invalid.temperatureC = 500.0;
  assert(!invalid.valid());

  std::cout << "ClientGpuTelemetryTests passed\n";
  return 0;
}
