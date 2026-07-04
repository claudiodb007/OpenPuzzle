#include "openpuzzle/core/CommandContext.hpp"

#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <cstdlib>

namespace openpuzzle {

bool CommandContext::initialize() {

  const char *home = std::getenv("HOME");

  if (!home) {
    lastError_ = "HOME not set";
    return false;
  }

  std::string dbPath =
      std::string(home) + "/.local/share/OpenPuzzle/openpuzzle.db";

  if (!db.open(dbPath)) {
    lastError_ = "Could not open database";
    return false;
  }

  if (!db.createSchema()) {
    lastError_ = "Could not create database schema";
    return false;
  }

  gpu = GpuManager::selectedGpu();

  bitcrack = ToolManager::bitcrackPath();

  return true;
}

const std::string &CommandContext::lastError() const { return lastError_; }

} // namespace openpuzzle
