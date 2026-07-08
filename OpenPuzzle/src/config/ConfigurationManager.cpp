#include "openpuzzle/config/ConfigurationManager.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

namespace fs = std::filesystem;

namespace openpuzzle {

static std::optional<std::string> readJsonStringAfterKey(const std::string& text,
                                                         const std::string& key) {
  auto k = text.find("\"" + key + "\"");

  if (k == std::string::npos)
    return std::nullopt;

  auto c = text.find(':', k);
  auto f = text.find('"', c);
  auto e = text.find('"', f + 1);

  if (c == std::string::npos || f == std::string::npos || e == std::string::npos)
    return std::nullopt;

  return text.substr(f + 1, e - f - 1);
}

static std::string readFile(const std::string& path) {
  std::ifstream in(path);

  if (!in)
    return {};

  std::stringstream buffer;
  buffer << in.rdbuf();

  return buffer.str();
}

std::string ConfigurationManager::configPath() {
  const char* home = std::getenv("HOME");

  fs::path path = home ? fs::path(home) : fs::current_path();

  path /= ".config/OpenPuzzle/config.json";

  return path.string();
}

Configuration ConfigurationManager::load() {
  Configuration config;

  const auto text = readFile(configPath());

  if (text.empty())
    return config;

  auto cudaPath = readJsonStringAfterKey(text, "cuda");

  if (cudaPath) {
    config.bitcrack.cudaPath = *cudaPath;
  } else {
    auto legacyBitCrack = readJsonStringAfterKey(text, "bitcrack");

    if (legacyBitCrack)
      config.bitcrack.cudaPath = *legacyBitCrack;
  }

  auto openclPath = readJsonStringAfterKey(text, "opencl");

  if (openclPath) {
    config.bitcrack.openclPath = *openclPath;
  } else if (!config.bitcrack.cudaPath.empty()) {
    fs::path cudaExecutable(config.bitcrack.cudaPath);
    config.bitcrack.openclPath =
        (cudaExecutable.parent_path() / "clBitCrack").string();
  }

  return config;
}

bool ConfigurationManager::save(const Configuration& config) {
  fs::path path = configPath();
  fs::create_directories(path.parent_path());

  std::ofstream out(path);

  if (!out)
    return false;

  out << "{\n";
  out << "  \"bitcrack\": {\n";
  out << "    \"cuda\": \"" << config.bitcrack.cudaPath << "\",\n";
  out << "    \"opencl\": \"" << config.bitcrack.openclPath << "\"\n";
  out << "  },\n";
  out << "  \"gpu_device\": " << config.gpu.device << "\n";
  out << "}\n";

  return true;
}

} // namespace openpuzzle
