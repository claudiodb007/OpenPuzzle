#include "openpuzzle/runtime/RuntimeStatusTelemetry.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace openpuzzle;

namespace {

GpuTelemetrySnapshot reading(
    const std::string &vendor,
    const std::string &deviceId,
    const int device,
    const double temperature,
    const double powerDraw,
    const double powerLimit) {
  GpuTelemetrySnapshot snapshot;
  snapshot.vendor = vendor;
  snapshot.deviceId = deviceId;
  snapshot.device = device;
  snapshot.temperatureC = temperature;
  snapshot.powerDrawW = powerDraw;
  snapshot.powerLimitW = powerLimit;
  return snapshot;
}

} // namespace

int main() {
  const std::vector<GpuTelemetrySnapshot> cudaReadings{
      reading("NVIDIA", "cuda-0", 0, 72.0, 289.2, 290.0),
      reading("NVIDIA", "cuda-4", 4, 60.0, 279.4, 280.0),
  };

  const auto cuda = RuntimeStatusTelemetry::select(
      "CUDA",
      4,
      "NVIDIA GeForce RTX 3080",
      cudaReadings);
  assert(cuda);
  assert(cuda->deviceId == "cuda-4");
  assert(RuntimeStatusTelemetry::temperatureText(*cuda) == "60.0 C");
  assert(RuntimeStatusTelemetry::powerText(*cuda) ==
         "279.4 W / 280.0 W");

  GpuTelemetrySnapshot amd =
      reading("AMD", "card1", 1, 81.0, 174.5, 220.0);
  amd.temperatureLabel = "junction";

  const auto opencl = RuntimeStatusTelemetry::select(
      "opencl",
      0,
      "AMD Radeon RX 5700 XT",
      {cudaReadings[0], amd});
  assert(opencl);
  assert(opencl->deviceId == "card1");
  assert(RuntimeStatusTelemetry::temperatureText(*opencl) ==
         "81.0 C (junction)");
  assert(RuntimeStatusTelemetry::powerText(*opencl) ==
         "174.5 W / 220.0 W");

  const auto ambiguous = RuntimeStatusTelemetry::select(
      "OpenCL",
      0,
      "AMD Radeon RX 5700 XT",
      {
          amd,
          reading("AMD", "card2", 2, 67.0, 120.0, 180.0),
      });
  assert(!ambiguous);

  assert(!RuntimeStatusTelemetry::select(
      "CPU",
      0,
      "",
      cudaReadings));
  assert(!RuntimeStatusTelemetry::select(
      "OpenCL",
      0,
      "Generic GPU",
      {amd}));

  GpuTelemetrySnapshot limitOnly;
  limitOnly.powerLimitW = 150.0;
  assert(RuntimeStatusTelemetry::temperatureText(limitOnly).empty());
  assert(RuntimeStatusTelemetry::powerText(limitOnly) == "limit 150.0 W");

  std::cout << "RuntimeStatusTelemetryTests passed\n";
  return 0;
}
