#pragma once

#include "openpuzzle/hardware/GpuTelemetry.hpp"

#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

class RuntimeStatusTelemetry {
public:
  /*
   * Match one physical sensor snapshot to a persisted runtime selection.
   * CUDA has an exact device-index contract. OpenCL is deliberately
   * conservative: a reading is returned only when the runtime GPU vendor has
   * exactly one physical telemetry candidate.
   */
  static std::optional<GpuTelemetrySnapshot> select(
      std::string backend,
      int device,
      const std::string &gpuName,
      const std::vector<GpuTelemetrySnapshot> &snapshots);

  static std::string temperatureText(
      const GpuTelemetrySnapshot &snapshot);

  static std::string powerText(
      const GpuTelemetrySnapshot &snapshot);
};

} // namespace openpuzzle
