#pragma once

#include "openpuzzle/config/Configuration.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class ConfigurationManager {
public:
  static std::string configPath();

  static Configuration load();

  // Missing files use defaults; existing invalid or unreadable files fail.
  // Use this result before read/modify/write operations.
  static std::optional<Configuration> loadChecked();

  static bool save(const Configuration& config);
};

} // namespace openpuzzle
