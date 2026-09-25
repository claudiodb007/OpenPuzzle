#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

using namespace openpuzzle;

namespace {

GpuTelemetrySnapshot snapshot(
    const int device,
    const double temperature) {
  GpuTelemetrySnapshot result;
  result.vendor = "NVIDIA";
  result.device = device;
  result.deviceId = "cuda-" + std::to_string(device);
  result.temperatureC = temperature;
  return result;
}

} // namespace

int main() {
  using namespace std::chrono_literals;

  unsetenv(RuntimeThermalObserver::OwnerEnvironment);

  const auto cuda =
      RuntimeThermalObserver::executionScope("CUDA", 2);
  assert(cuda);
  assert(cuda->size() == 1);
  assert(cuda->front() == "cuda-2");

  const auto cpu =
      RuntimeThermalObserver::executionScope("cpu", 0);
  assert(cpu);
  assert(cpu->empty());

  const auto opencl =
      RuntimeThermalObserver::executionScope("opencl", 1);
  assert(!opencl);

  const auto selected =
      RuntimeThermalObserver::cudaScope({1, -1, 4});
  assert(selected);
  assert(selected->size() == 2);
  assert((*selected)[0] == "cuda-1");
  assert((*selected)[1] == "cuda-4");

  GpuThermalPolicyConfiguration policy;
  policy.enabled = true;
  policy.stopOnCritical = true;
  policy.warningC = 75.0;
  policy.criticalC = 85.0;

  int reads = 0;
  RuntimeThermalObserver observer(
      policy,
      RuntimeThermalObserver::cudaScope({1}),
      [&] {
        ++reads;
        return std::vector<GpuTelemetrySnapshot>{
            snapshot(0, 95.0),
            snapshot(1, 70.0)};
      },
      1s,
      5min);

  const RuntimeThermalObserver::Clock::time_point start{};
  assert(observer.pollAt(start).empty());
  assert(reads == 1);

  RuntimeThermalObserver criticalObserver(
      policy,
      RuntimeThermalObserver::cudaScope({1}),
      [] {
        return std::vector<GpuTelemetrySnapshot>{
            snapshot(0, 70.0),
            snapshot(1, 90.0)};
      },
      1s,
      5min);

  const auto critical = criticalObserver.pollAt(start);
  assert(critical.size() == 1);
  assert(critical[0].kind == RuntimeThermalEventKind::Critical);
  assert(critical[0].snapshot.deviceId == "cuda-1");

  int cpuReads = 0;
  RuntimeThermalObserver cpuObserver(
      policy,
      cpu,
      [&] {
        ++cpuReads;
        return std::vector<GpuTelemetrySnapshot>{snapshot(0, 90.0)};
      });

  assert(!cpuObserver.enabled());
  assert(cpuObserver.pollAt(start).empty());
  assert(cpuReads == 0);

  return 0;
}
