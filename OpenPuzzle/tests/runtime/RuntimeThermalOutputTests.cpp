#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include <cassert>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

using namespace openpuzzle;

namespace {

GpuTelemetrySnapshot snapshot(const double temperature) {
  GpuTelemetrySnapshot result;
  result.vendor = "AMD";
  result.deviceId = "card2";
  result.temperatureC = temperature;
  return result;
}

std::string criticalOutput(const bool stopOnCritical) {
  GpuThermalPolicyConfiguration policy;
  policy.enabled = true;
  policy.stopOnCritical = stopOnCritical;
  policy.warningC = 75.0;
  policy.criticalC = 85.0;

  RuntimeThermalObserver observer(
      policy,
      [] {
        return std::vector<GpuTelemetrySnapshot>{
            snapshot(90.0)};
      },
      std::chrono::seconds(1),
      std::chrono::minutes(5));

  const auto events = observer.pollAt({});
  assert(events.size() == 1);
  assert(events.front().kind ==
         RuntimeThermalEventKind::Critical);
  assert(events.front().orderlyStop == stopOnCritical);

  std::ostringstream output;
  RuntimeThermalObserver::print(events.front(), output);
  return output.str();
}

} // namespace

int main() {
  const std::string diagnostic = criticalOutput(false);
  assert(diagnostic.find(
             "diagnostic only; execution continues") !=
         std::string::npos);
  assert(diagnostic.find("orderly stop requested") ==
         std::string::npos);

  const std::string protectedOutput = criticalOutput(true);
  assert(protectedOutput.find("orderly stop requested") !=
         std::string::npos);
  assert(protectedOutput.find(
             "diagnostic only; execution continues") ==
         std::string::npos);

  return 0;
}
