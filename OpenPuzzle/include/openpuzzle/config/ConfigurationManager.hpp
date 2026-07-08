#pragma once

#include "openpuzzle/config/Configuration.hpp"

#include <string>

namespace openpuzzle {

class ConfigurationManager {
public:
  static std::string configPath();

  static Configuration load();

  static bool save(const Configuration& config);
};

} // namespace openpuzzle
