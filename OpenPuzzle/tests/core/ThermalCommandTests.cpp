#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/core/commands/ThermalCommand.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

using namespace openpuzzle;

int main() {
  const char *oldHome = std::getenv("HOME");
  const std::string savedHome = oldHome ? oldHome : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-thermal-command-" +
       std::to_string(static_cast<long long>(getpid())));

  fs::remove_all(root);
  fs::create_directories(root);

  if (setenv("HOME", root.c_str(), 1) != 0) {
    return 1;
  }

  ThermalCommand command;

  if (command.run({}) != 0 ||
      fs::exists(ConfigurationManager::configPath())) {
    return 2;
  }

  if (command.run({
          "--enable",
          "--warning-c", "72.5",
          "--critical-c", "84.5"}) != 0) {
    return 3;
  }

  auto configuration = ConfigurationManager::load();
  if (!configuration.gpu.thermal.enabled ||
      configuration.gpu.thermal.warningC != 72.5 ||
      configuration.gpu.thermal.criticalC != 84.5) {
    return 4;
  }

  if (command.run({
          "--warning-c", "90",
          "--critical-c", "80"}) == 0) {
    return 5;
  }

  configuration = ConfigurationManager::load();
  if (configuration.gpu.thermal.warningC != 72.5 ||
      configuration.gpu.thermal.criticalC != 84.5) {
    return 6;
  }

  if (command.run({"--enable", "--disable"}) == 0 ||
      command.run({"--unknown"}) == 0 ||
      command.run({"--warning-c"}) == 0 ||
      command.run({"--critical-c", "nan"}) == 0) {
    return 7;
  }

  if (command.run({"--disable"}) != 0) {
    return 8;
  }

  configuration = ConfigurationManager::load();
  if (configuration.gpu.thermal.enabled ||
      configuration.gpu.thermal.warningC != 72.5 ||
      configuration.gpu.thermal.criticalC != 84.5) {
    return 9;
  }

  if (command.run({"--help"}) != 0) {
    return 10;
  }

  if (oldHome) {
    if (setenv("HOME", savedHome.c_str(), 1) != 0) {
      return 11;
    }
  } else if (unsetenv("HOME") != 0) {
    return 12;
  }

  fs::remove_all(root);
  return 0;
}
