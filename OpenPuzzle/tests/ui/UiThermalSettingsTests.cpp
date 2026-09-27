#include "ThermalSettings.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

int main() {
  using namespace openpuzzle;
  using namespace openpuzzle::ui;

  const char* previousHome = std::getenv("HOME");
  const std::string savedHome =
      previousHome ? previousHome : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-ui-thermal-settings-" +
       std::to_string(static_cast<long long>(getpid())));

  fs::remove_all(root);
  fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);

  Configuration initial;
  initial.engine.id = "bitcrack";
  initial.gpu.device = 7;
  initial.assignment.durationMinutes = 42;
  assert(ConfigurationManager::save(initial));

  const auto defaults = ThermalSettingsStore::load();
  assert(!defaults.enabled);
  assert(!defaults.stopOnCritical);
  assert(defaults.warningC == 75.0);
  assert(defaults.criticalC == 85.0);

  ThermalSettings invalid = defaults;
  invalid.warningC = 90.0;
  invalid.criticalC = 80.0;
  assert(!ThermalSettingsStore::valid(invalid));
  assert(ThermalSettingsStore::save(invalid) ==
         ThermalSettingsSaveResult::Invalid);

  ThermalSettings configured;
  configured.enabled = true;
  configured.stopOnCritical = true;
  configured.warningC = 72.5;
  configured.criticalC = 84.5;
  assert(ThermalSettingsStore::valid(configured));
  assert(ThermalSettingsStore::save(configured) ==
         ThermalSettingsSaveResult::Saved);

  const auto loaded = ThermalSettingsStore::load();
  assert(loaded.enabled);
  assert(loaded.stopOnCritical);
  assert(loaded.warningC == 72.5);
  assert(loaded.criticalC == 84.5);

  const auto preserved = ConfigurationManager::load();
  assert(preserved.engine.id == "bitcrack");
  assert(preserved.gpu.device == 7);
  assert(preserved.assignment.durationMinutes == 42);

  if (previousHome) {
    assert(setenv("HOME", savedHome.c_str(), 1) == 0);
  } else {
    assert(unsetenv("HOME") == 0);
  }

  fs::remove_all(root);
  return 0;
}
