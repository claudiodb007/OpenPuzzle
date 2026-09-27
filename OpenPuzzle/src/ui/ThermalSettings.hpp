#pragma once

namespace openpuzzle::ui {

struct ThermalSettings {
  bool enabled = false;
  bool stopOnCritical = false;
  double warningC = 75.0;
  double criticalC = 85.0;
};

enum class ThermalSettingsSaveResult {
  Saved,
  Invalid,
  Failed,
};

class ThermalSettingsStore {
public:
  static ThermalSettings load();

  static bool valid(const ThermalSettings& settings);

  static ThermalSettingsSaveResult save(
      const ThermalSettings& settings);
};

} // namespace openpuzzle::ui
