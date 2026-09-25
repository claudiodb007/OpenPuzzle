#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

struct GpuTelemetrySnapshot {
  std::string vendor;
  std::string deviceId;
  std::string pciBusId;
  std::string temperatureLabel;

  int device = -1;

  std::optional<double> temperatureC;
  std::optional<double> powerDrawW;
  std::optional<double> powerLimitW;

  bool hasMeasurements() const;
};

class GpuTelemetry {
public:
  /*
   * Read-only telemetry discovery. Missing tools, sensors and individual
   * values are represented by an empty result or std::nullopt; telemetry
   * collection never changes clocks, fans, power limits or runtime state.
   */
  static std::vector<GpuTelemetrySnapshot> readAll();
  static std::vector<GpuTelemetrySnapshot> readNvidia();
  static std::vector<GpuTelemetrySnapshot> readAmd(
      const std::filesystem::path &drmRoot = "/sys/class/drm");

  /* Public for deterministic parser contract tests. */
  static std::vector<GpuTelemetrySnapshot> parseNvidiaSmi(
      const std::string &output);
};

} // namespace openpuzzle
