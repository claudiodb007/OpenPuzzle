#include "openpuzzle/config/ConfigurationManager.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

int main() {
  const char *oldHome = std::getenv("HOME");
  const std::string savedHome = oldHome ? oldHome : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-thermal-config-" +
       std::to_string(static_cast<long long>(getpid())));

  fs::remove_all(root);
  fs::create_directories(root);

  if (setenv("HOME", root.c_str(), 1) != 0) {
    return 1;
  }

  const auto defaults = openpuzzle::ConfigurationManager::load();
  if (defaults.gpu.thermal.enabled ||
      defaults.gpu.thermal.stopOnCritical ||
      defaults.gpu.thermal.warningC != 75.0 ||
      defaults.gpu.thermal.criticalC != 85.0) {
    return 2;
  }

  openpuzzle::Configuration configuration;
  configuration.gpu.thermal.enabled = true;
  configuration.gpu.thermal.stopOnCritical = true;
  configuration.gpu.thermal.warningC = 72.5;
  configuration.gpu.thermal.criticalC = 84.5;

  if (!openpuzzle::ConfigurationManager::save(configuration)) {
    return 3;
  }

  const auto loaded = openpuzzle::ConfigurationManager::load();
  if (!loaded.gpu.thermal.enabled ||
      !loaded.gpu.thermal.stopOnCritical ||
      loaded.gpu.thermal.warningC != 72.5 ||
      loaded.gpu.thermal.criticalC != 84.5) {
    return 4;
  }

  std::ifstream input(openpuzzle::ConfigurationManager::configPath());
  std::stringstream text;
  text << input.rdbuf();

  if (text.str().find("\"thermal_enabled\": true") ==
          std::string::npos ||
      text.str().find("\"thermal_stop_on_critical\": true") ==
          std::string::npos ||
      text.str().find("\"thermal_warning_c\": 72.5") ==
          std::string::npos ||
      text.str().find("\"thermal_critical_c\": 84.5") ==
          std::string::npos) {
    return 5;
  }

  if (oldHome) {
    if (setenv("HOME", savedHome.c_str(), 1) != 0) {
      return 6;
    }
  } else if (unsetenv("HOME") != 0) {
    return 7;
  }

  fs::remove_all(root);
  return 0;
}
