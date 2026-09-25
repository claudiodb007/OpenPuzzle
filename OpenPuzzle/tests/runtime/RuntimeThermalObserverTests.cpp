#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

using namespace openpuzzle;

namespace {

GpuTelemetrySnapshot snapshot(const double temperature) {
  GpuTelemetrySnapshot result;
  result.vendor = "NVIDIA";
  result.deviceId = "cuda-2";
  result.device = 2;
  result.temperatureC = temperature;
  return result;
}

} // namespace

int main() {
  using namespace std::chrono_literals;

  unsetenv(RuntimeThermalObserver::OwnerEnvironment);

  GpuThermalPolicyConfiguration disabled;
  int disabledReads = 0;
  RuntimeThermalObserver disabledObserver(
      disabled,
      [&] {
        ++disabledReads;
        return std::vector<GpuTelemetrySnapshot>{snapshot(90.0)};
      });

  assert(disabledObserver.pollAt({}).empty());
  assert(disabledReads == 0);

  GpuThermalPolicyConfiguration policy;
  policy.enabled = true;
  policy.warningC = 75.0;
  policy.criticalC = 85.0;

  int ownedReads = 0;
  RuntimeThermalObserver childObserver(
      policy,
      [&] {
        ++ownedReads;
        return std::vector<GpuTelemetrySnapshot>{snapshot(80.0)};
      });

  assert(
      setenv(
          RuntimeThermalObserver::OwnerEnvironment,
          "0",
          1) == 0);
  assert(!RuntimeThermalObserver::processOwnsMonitoring());
  assert(childObserver.pollAt({}).empty());
  assert(ownedReads == 0);
  assert(unsetenv(RuntimeThermalObserver::OwnerEnvironment) == 0);
  assert(RuntimeThermalObserver::processOwnsMonitoring());

  double temperature = 80.0;
  int reads = 0;
  RuntimeThermalObserver observer(
      policy,
      [&] {
        ++reads;
        return std::vector<GpuTelemetrySnapshot>{snapshot(temperature)};
      },
      30s,
      5min);

  const RuntimeThermalObserver::Clock::time_point start{};

  auto events = observer.pollAt(start);
  assert(reads == 1);
  assert(events.size() == 1);
  assert(events[0].kind == RuntimeThermalEventKind::Warning);
  assert(!events[0].reminder);
  assert(events[0].thresholdC == 75.0);

  assert(observer.pollAt(start + 29s).empty());
  assert(reads == 1);

  assert(observer.pollAt(start + 30s).empty());
  assert(reads == 2);

  events = observer.pollAt(start + 5min);
  assert(reads == 3);
  assert(events.size() == 1);
  assert(events[0].kind == RuntimeThermalEventKind::Warning);
  assert(events[0].reminder);

  temperature = 86.0;
  events = observer.pollAt(start + 5min + 30s);
  assert(events.size() == 1);
  assert(events[0].kind == RuntimeThermalEventKind::Critical);
  assert(!events[0].reminder);
  assert(events[0].thresholdC == 85.0);

  temperature = 70.0;
  events = observer.pollAt(start + 6min);
  assert(events.size() == 1);
  assert(events[0].kind == RuntimeThermalEventKind::Recovered);

  std::ostringstream output;
  RuntimeThermalObserver::print(events[0], output);
  assert(output.str().find("OpenPuzzle thermal recovery") !=
         std::string::npos);
  assert(output.str().find("State............... NORMAL") !=
         std::string::npos);
  assert(output.str().find("monitoring continues") !=
         std::string::npos);

  temperature = 90.0;
  events = observer.pollAt(start + 6min + 30s);
  assert(events.size() == 1);
  output.str({});
  output.clear();
  RuntimeThermalObserver::print(events[0], output);
  assert(output.str().find("State............... CRITICAL") !=
         std::string::npos);
  assert(output.str().find("diagnostic only; execution continues") !=
         std::string::npos);

  return 0;
}
