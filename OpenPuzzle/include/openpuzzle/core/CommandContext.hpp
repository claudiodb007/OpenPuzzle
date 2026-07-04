#pragma once

#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class CommandContext {
public:
  Database db;
  Scheduler scheduler;

  int gpu = 0;
  std::optional<std::string> bitcrack;

private:
  std::string lastError_;

  bool initialize();

  const std::string &lastError() const;
};

} // namespace openpuzzle
