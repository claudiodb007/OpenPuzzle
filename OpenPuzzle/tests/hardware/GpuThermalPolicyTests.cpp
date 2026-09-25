#include "openpuzzle/hardware/GpuThermalPolicy.hpp"

#include <limits>
#include <optional>

using namespace openpuzzle;

int main() {
  GpuThermalPolicyConfiguration policy;

  if (policy.enabled ||
      policy.stopOnCritical ||
      policy.warningC != 75.0 ||
      policy.criticalC != 85.0 ||
      !GpuThermalPolicy::valid(policy)) {
    return 1;
  }

  if (GpuThermalPolicy::evaluate(policy, 90.0) !=
      GpuThermalState::Disabled) {
    return 2;
  }

  policy.enabled = true;

  if (GpuThermalPolicy::evaluate(policy, std::nullopt) !=
      GpuThermalState::Unavailable) {
    return 3;
  }

  if (GpuThermalPolicy::evaluate(policy, 74.9) !=
      GpuThermalState::Normal) {
    return 4;
  }

  if (GpuThermalPolicy::evaluate(policy, 75.0) !=
      GpuThermalState::Warning) {
    return 5;
  }

  if (GpuThermalPolicy::evaluate(policy, 84.9) !=
      GpuThermalState::Warning) {
    return 6;
  }

  if (GpuThermalPolicy::evaluate(policy, 85.0) !=
      GpuThermalState::Critical) {
    return 7;
  }

  policy.warningC = 90.0;
  policy.criticalC = 85.0;

  if (GpuThermalPolicy::valid(policy) ||
      GpuThermalPolicy::evaluate(policy, 70.0) !=
          GpuThermalState::Invalid) {
    return 8;
  }

  policy.warningC = std::numeric_limits<double>::quiet_NaN();
  policy.criticalC = 100.0;

  if (GpuThermalPolicy::valid(policy)) {
    return 9;
  }

  if (GpuThermalPolicy::stateName(GpuThermalState::Critical) !=
          "CRITICAL" ||
      GpuThermalPolicy::stateName(GpuThermalState::Disabled) !=
          "DISABLED") {
    return 10;
  }

  return 0;
}
