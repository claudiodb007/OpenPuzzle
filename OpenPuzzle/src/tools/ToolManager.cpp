#include "openpuzzle/tools/ToolManager.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>

namespace fs = std::filesystem;

namespace openpuzzle {

std::string ToolManager::configPath() {
  return ConfigurationManager::configPath();
}

bool ToolManager::configureBitCrack(const std::string &path) {
  Configuration config = ConfigurationManager::load();

  config.bitcrack.cudaPath = path;

  if (config.bitcrack.openclPath.empty()) {
    fs::path cudaExecutable(path);
    config.bitcrack.openclPath =
        (cudaExecutable.parent_path() / "clBitCrack").string();
  }

  return ConfigurationManager::save(config);
}

bool ToolManager::configureBitCrack(const std::string &cudaPath,
                                    const std::string &openclPath) {
  Configuration config = ConfigurationManager::load();

  config.bitcrack.cudaPath = cudaPath;
  config.bitcrack.openclPath = openclPath;

  return ConfigurationManager::save(config);
}

std::optional<std::string> ToolManager::bitcrackPath() {
  return bitcrackCudaPath();
}

std::optional<std::string> ToolManager::bitcrackCudaPath() {
  auto config = ConfigurationManager::load();

  if (config.bitcrack.cudaPath.empty())
    return std::nullopt;

  return config.bitcrack.cudaPath;
}

std::optional<std::string> ToolManager::bitcrackOpenCLPath() {
  auto config = ConfigurationManager::load();

  if (config.bitcrack.openclPath.empty())
    return std::nullopt;

  return config.bitcrack.openclPath;
}

} // namespace openpuzzle
