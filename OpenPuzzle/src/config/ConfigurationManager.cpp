#include "openpuzzle/config/ConfigurationManager.hpp"

#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

std::optional<std::string> readJsonStringAfterKey(const std::string &text,
                                                  const std::string &key) {
  const auto keyPosition = text.find("\"" + key + "\"");

  if (keyPosition == std::string::npos) {
    return std::nullopt;
  }

  const auto colon = text.find(':', keyPosition);

  const auto firstQuote = text.find('"', colon);

  const auto lastQuote = text.find('"', firstQuote + 1);

  if (colon == std::string::npos || firstQuote == std::string::npos ||
      lastQuote == std::string::npos) {
    return std::nullopt;
  }

  return text.substr(firstQuote + 1, lastQuote - firstQuote - 1);
}

std::optional<int> readJsonIntegerAfterKey(const std::string &text,
                                           const std::string &key) {
  const auto keyPosition = text.find("\"" + key + "\"");

  if (keyPosition == std::string::npos) {
    return std::nullopt;
  }

  const auto colon = text.find(':', keyPosition);

  if (colon == std::string::npos) {
    return std::nullopt;
  }

  std::size_t position = colon + 1;

  while (position < text.size() &&
         std::isspace(static_cast<unsigned char>(text[position]))) {
    ++position;
  }

  std::size_t consumed = 0;

  try {
    const int value = std::stoi(text.substr(position), &consumed);

    return value;
  } catch (...) {
    return std::nullopt;
  }
}

std::string readFile(const std::string &path) {
  std::ifstream input(path);

  if (!input) {
    return {};
  }

  std::stringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
}

std::string escapeJson(const std::string &value) {
  std::string escaped;

  for (const char character : value) {
    if (character == '\\' || character == '"') {
      escaped.push_back('\\');
    }

    escaped.push_back(character);
  }

  return escaped;
}

} // namespace

std::string ConfigurationManager::configPath() {
  const char *home = std::getenv("HOME");

  fs::path path = home ? fs::path(home) : fs::current_path();

  path /= ".config/OpenPuzzle/config.json";

  return path.string();
}

Configuration ConfigurationManager::load() {
  Configuration config;

  const fs::path path =
      configPath();

  try {
    WorkspaceSecurity::prepare(
        path.parent_path());

    if (fs::is_regular_file(
            path)) {
      WorkspaceSecurity::protectFile(
          path);
    }
  } catch (...) {
    return config;
  }

  const auto text =
      readFile(
          path.string());

  if (text.empty()) {
    return config;
  }

  if (const auto value = readJsonStringAfterKey(text, "cuda")) {
    config.bitcrack.cudaPath = *value;
  } else if (const auto legacy = readJsonStringAfterKey(text, "bitcrack")) {
    config.bitcrack.cudaPath = *legacy;
  }

  if (const auto value = readJsonStringAfterKey(text, "opencl")) {
    config.bitcrack.openclPath = *value;
  } else if (!config.bitcrack.cudaPath.empty()) {
    config.bitcrack.openclPath =
        (fs::path(config.bitcrack.cudaPath).parent_path() / "clBitCrack")
            .string();
  }

  if (const auto value = readJsonStringAfterKey(text, "engine_id")) {
    config.engine.id = *value;
  }

  if (const auto value = readJsonStringAfterKey(text, "backend")) {
    config.engine.backend = *value;
  }

  if (const auto value = readJsonStringAfterKey(text, "executable")) {
    config.engine.executable = *value;
  }

  if (const auto value = readJsonIntegerAfterKey(text, "gpu_device")) {
    config.gpu.device = *value;
  }

  if (const auto value = readJsonIntegerAfterKey(text, "duration_minutes")) {
    if (*value > 0) {
      config.assignment.durationMinutes = *value;
    }
  }

  return config;
}

bool ConfigurationManager::save(const Configuration &config) {
  const fs::path path = configPath();

  try {
    WorkspaceSecurity::prepare(
        path.parent_path());
  } catch (...) {
    return false;
  }

  std::ofstream output(
      path,
      std::ios::trunc);

  if (!output) {
    return false;
  }

  try {
    WorkspaceSecurity::protectFile(
        path);
  } catch (...) {
    return false;
  }

  output << "{\n"
         << "  \"engine\": {\n"
         << "    \"engine_id\": \"" << escapeJson(config.engine.id) << "\",\n"
         << "    \"backend\": \"" << escapeJson(config.engine.backend)
         << "\",\n"
         << "    \"executable\": \"" << escapeJson(config.engine.executable)
         << "\"\n"
         << "  },\n"
         << "  \"bitcrack\": {\n"
         << "    \"cuda\": \"" << escapeJson(config.bitcrack.cudaPath)
         << "\",\n"
         << "    \"opencl\": \"" << escapeJson(config.bitcrack.openclPath)
         << "\"\n"
         << "  },\n"
         << "  \"gpu_device\": " << config.gpu.device << ",\n"
         << "  \"assignment\": {\n"
         << "    \"duration_minutes\": " << config.assignment.durationMinutes
         << "\n"
         << "  }\n"
         << "}\n";

  output.close();

  return static_cast<bool>(
      output);
}

} // namespace openpuzzle
