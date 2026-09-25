#pragma once

#include "openpuzzle/hardware/GpuTelemetry.hpp"
#include "openpuzzle/hardware/GpuThermalPolicy.hpp"

#include <chrono>
#include <functional>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

enum class RuntimeThermalEventKind {
  Warning,
  Critical,
  Recovered,
};

struct RuntimeThermalEvent {
  RuntimeThermalEventKind kind =
      RuntimeThermalEventKind::Warning;
  GpuTelemetrySnapshot snapshot;
  double thresholdC = 0.0;
  bool reminder = false;
};

class RuntimeThermalObserver {
public:
  using Clock = std::chrono::steady_clock;
  using Reader =
      std::function<std::vector<GpuTelemetrySnapshot>()>;

  static constexpr const char *OwnerEnvironment =
      "OPENPUZZLE_THERMAL_MONITOR_OWNER";

  explicit RuntimeThermalObserver(
      GpuThermalPolicyConfiguration policy,
      Reader reader = GpuTelemetry::readAll,
      std::chrono::seconds sampleInterval =
          std::chrono::seconds(30),
      std::chrono::seconds reminderInterval =
          std::chrono::minutes(5));

  bool enabled() const;

  std::vector<RuntimeThermalEvent> poll();

  std::vector<RuntimeThermalEvent> pollAt(
      Clock::time_point now);

  static bool processOwnsMonitoring();

  static void print(
      const RuntimeThermalEvent &event,
      std::ostream &output);

private:
  struct DeviceState {
    GpuThermalState state =
        GpuThermalState::Unavailable;
    std::optional<Clock::time_point> lastNotice;
  };

  GpuThermalPolicyConfiguration policy_;
  Reader reader_;
  std::chrono::seconds sampleInterval_;
  std::chrono::seconds reminderInterval_;
  std::optional<Clock::time_point> nextSample_;
  std::map<std::string, DeviceState> states_;
};

} // namespace openpuzzle
