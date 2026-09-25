#pragma once

#include <optional>
#include <string>

namespace openpuzzle {

struct GpuThermalPolicyConfiguration {
  bool enabled = false;
  double warningC = 75.0;
  double criticalC = 85.0;
};

enum class GpuThermalState {
  Disabled,
  Unavailable,
  Normal,
  Warning,
  Critical,
  Invalid,
};

class GpuThermalPolicy {
public:
  static bool valid(
      const GpuThermalPolicyConfiguration &configuration);

  static GpuThermalState evaluate(
      const GpuThermalPolicyConfiguration &configuration,
      const std::optional<double> &temperatureC);

  static std::string stateName(GpuThermalState state);
};

} // namespace openpuzzle
