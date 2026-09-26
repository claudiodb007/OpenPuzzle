#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace openpuzzle;

namespace {

GpuInfo gpu(
    const int device,
    const std::string &name) {
  GpuInfo result;
  result.device = device;
  result.name = name;
  result.backend = "OpenCL";
  result.opencl = true;
  return result;
}

GpuTelemetrySnapshot reading(
    const std::string &vendor,
    const std::string &deviceId) {
  GpuTelemetrySnapshot result;
  result.vendor = vendor;
  result.deviceId = deviceId;
  result.temperatureC = 60.0;
  return result;
}

} // namespace

int main() {
  const std::vector<GpuInfo> mixedDevices = {
      gpu(0, "NVIDIA GeForce RTX 4070 SUPER"),
      gpu(1, "AMD Radeon RX 5500 XT (radeonsi)")};

  const std::vector<GpuTelemetrySnapshot> mixedTelemetry = {
      reading("NVIDIA", "cuda-0"),
      reading("AMD", "card2")};

  const auto amd = RuntimeThermalObserver::openclScope(
      mixedDevices,
      {1},
      mixedTelemetry);
  assert(amd);
  assert(amd->size() == 1);
  assert(amd->front() == "card2");

  const auto nvidia = RuntimeThermalObserver::openclScope(
      mixedDevices,
      {0},
      mixedTelemetry);
  assert(nvidia);
  assert(nvidia->size() == 1);
  assert(nvidia->front() == "cuda-0");

  const auto both = RuntimeThermalObserver::openclScope(
      mixedDevices,
      {0, 1},
      mixedTelemetry);
  assert(both);
  assert(both->size() == 2);

  const std::vector<GpuInfo> nvidiaDevices = {
      gpu(0, "NVIDIA GeForce RTX 3080"),
      gpu(1, "NVIDIA GeForce RTX 3080")};

  const std::vector<GpuTelemetrySnapshot> nvidiaTelemetry = {
      reading("NVIDIA", "cuda-0"),
      reading("NVIDIA", "cuda-1")};

  assert(!RuntimeThermalObserver::openclScope(
      nvidiaDevices,
      {1},
      nvidiaTelemetry));

  const auto allNvidia = RuntimeThermalObserver::openclScope(
      nvidiaDevices,
      {0, 1},
      nvidiaTelemetry);
  assert(allNvidia);
  assert(allNvidia->size() == 2);

  assert(!RuntimeThermalObserver::openclScope(
      {gpu(0, "Generic accelerator")},
      {0},
      mixedTelemetry));

  const auto combined = RuntimeThermalObserver::combineScopes(
      RuntimeThermalObserver::cudaScope({0}),
      amd);
  assert(combined);
  assert(combined->size() == 2);
  assert((*combined)[0] == "card2");
  assert((*combined)[1] == "cuda-0");

  assert(!RuntimeThermalObserver::combineScopes(
      std::nullopt,
      amd));

  return 0;
}
