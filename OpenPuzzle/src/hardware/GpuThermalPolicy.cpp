#include "openpuzzle/hardware/GpuThermalPolicy.hpp"

#include <cmath>

namespace openpuzzle {

bool GpuThermalPolicy::valid(
    const GpuThermalPolicyConfiguration &configuration) {
  return std::isfinite(configuration.warningC) &&
         std::isfinite(configuration.criticalC) &&
         configuration.warningC >= 30.0 &&
         configuration.warningC <= 110.0 &&
         configuration.criticalC > configuration.warningC &&
         configuration.criticalC <= 120.0;
}

GpuThermalState GpuThermalPolicy::evaluate(
    const GpuThermalPolicyConfiguration &configuration,
    const std::optional<double> &temperatureC) {
  if (!valid(configuration)) {
    return GpuThermalState::Invalid;
  }

  if (!configuration.enabled) {
    return GpuThermalState::Disabled;
  }

  if (!temperatureC || !std::isfinite(*temperatureC)) {
    return GpuThermalState::Unavailable;
  }

  if (*temperatureC >= configuration.criticalC) {
    return GpuThermalState::Critical;
  }

  if (*temperatureC >= configuration.warningC) {
    return GpuThermalState::Warning;
  }

  return GpuThermalState::Normal;
}

std::string GpuThermalPolicy::stateName(
    const GpuThermalState state) {
  switch (state) {
  case GpuThermalState::Disabled:
    return "DISABLED";
  case GpuThermalState::Unavailable:
    return "UNAVAILABLE";
  case GpuThermalState::Normal:
    return "NORMAL";
  case GpuThermalState::Warning:
    return "WARNING";
  case GpuThermalState::Critical:
    return "CRITICAL";
  case GpuThermalState::Invalid:
    return "INVALID";
  }

  return "INVALID";
}

} // namespace openpuzzle
