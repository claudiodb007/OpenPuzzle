#include "ThermalSettings.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/hardware/GpuThermalPolicy.hpp"

namespace openpuzzle::ui {

namespace {

GpuThermalPolicyConfiguration toPolicy(
    const ThermalSettings& settings) {
  GpuThermalPolicyConfiguration policy;
  policy.enabled = settings.enabled;
  policy.stopOnCritical = settings.stopOnCritical;
  policy.warningC = settings.warningC;
  policy.criticalC = settings.criticalC;
  return policy;
}

} // namespace

ThermalSettings ThermalSettingsStore::load() {
  const auto policy =
      ConfigurationManager::load().gpu.thermal;

  ThermalSettings settings;
  settings.enabled = policy.enabled;
  settings.stopOnCritical = policy.stopOnCritical;
  settings.warningC = policy.warningC;
  settings.criticalC = policy.criticalC;
  return settings;
}

bool ThermalSettingsStore::valid(
    const ThermalSettings& settings) {
  return GpuThermalPolicy::valid(toPolicy(settings));
}

ThermalSettingsSaveResult ThermalSettingsStore::save(
    const ThermalSettings& settings) {
  const auto policy = toPolicy(settings);
  if (!GpuThermalPolicy::valid(policy)) {
    return ThermalSettingsSaveResult::Invalid;
  }

  const auto loaded = ConfigurationManager::loadChecked();
  if (!loaded) {
    return ThermalSettingsSaveResult::Failed;
  }
  auto configuration = *loaded;
  configuration.gpu.thermal = policy;

  return ConfigurationManager::save(configuration)
      ? ThermalSettingsSaveResult::Saved
      : ThermalSettingsSaveResult::Failed;
}

} // namespace openpuzzle::ui
