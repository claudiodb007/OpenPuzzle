#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include <cassert>
#include <chrono>
#include <optional>
#include <vector>

using namespace openpuzzle;

namespace {

GpuTelemetrySnapshot snapshot(
    const std::optional<double> temperature) {
  GpuTelemetrySnapshot result;
  result.vendor = "NVIDIA";
  result.deviceId = "cuda-0";
  result.temperatureC = temperature;
  return result;
}

} // namespace

int main() {
  using namespace std::chrono_literals;

  static_assert(
      RuntimeThermalObserver::RecoveryHysteresisC == 2.0);

  GpuThermalPolicyConfiguration policy;
  policy.enabled = true;
  policy.warningC = 75.0;
  policy.criticalC = 85.0;

  std::optional<double> temperature = 76.0;
  RuntimeThermalObserver observer(
      policy,
      [&] {
        return std::vector<GpuTelemetrySnapshot>{
            snapshot(temperature)};
      },
      1s,
      5min);

  const RuntimeThermalObserver::Clock::time_point start{};

  auto events = observer.pollAt(start);
  assert(events.size() == 1);
  assert(events.front().kind ==
         RuntimeThermalEventKind::Warning);

  /* Below warning, but still inside the two-degree recovery margin. */
  temperature = 74.0;
  assert(observer.pollAt(start + 1s).empty());

  /* A missing sample must not erase the existing warning state. */
  temperature.reset();
  assert(observer.pollAt(start + 2s).empty());

  /* The exact lower boundary completes recovery. */
  temperature = 73.0;
  events = observer.pollAt(start + 3s);
  assert(events.size() == 1);
  assert(events.front().kind ==
         RuntimeThermalEventKind::Recovered);

  /* Crossing the configured warning threshold still warns normally. */
  temperature = 75.0;
  events = observer.pollAt(start + 4s);
  assert(events.size() == 1);
  assert(events.front().kind ==
         RuntimeThermalEventKind::Warning);

  return 0;
}
