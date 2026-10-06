#include "openpuzzle/config/ConfigurationManager.hpp"

#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/JsonString.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

std::optional<std::string> findJsonString(
    const boost::property_tree::ptree& node,
    const std::string& key) {
  for (const auto& field : node) {
    if (field.first == key && field.second.empty()) {
      return field.second.data();
    }
    if (const auto value = findJsonString(field.second, key)) {
      return value;
    }
  }
  return std::nullopt;
}

std::optional<std::string> readJsonStringAfterKey(
    const std::string& text,
    const std::string& key) {
  try {
    std::istringstream input(text);
    boost::property_tree::ptree document;
    boost::property_tree::read_json(input, document);
    return findJsonString(document, key);
  } catch (...) {
    return std::nullopt;
  }
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

std::optional<double> readJsonDoubleAfterKey(
    const std::string &text,
    const std::string &key) {
  const auto keyPosition = text.find("\"" + key + "\"");

  if (keyPosition == std::string::npos) {
    return std::nullopt;
  }

  const auto colon = text.find(':', keyPosition);
  if (colon == std::string::npos) {
    return std::nullopt;
  }

  std::istringstream input(text.substr(colon + 1));
  input.imbue(std::locale::classic());

  double value = 0.0;
  if (!(input >> value)) {
    return std::nullopt;
  }

  return value;
}

std::optional<bool> readJsonBooleanAfterKey(
    const std::string &text,
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

  if (text.compare(position, 4, "true") == 0) {
    return true;
  }

  if (text.compare(position, 5, "false") == 0) {
    return false;
  }

  return std::nullopt;
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

  if (const auto value = readJsonStringAfterKey(text, "rusticl_enable")) {
    config.gpu.rusticlEnable = *value;
  }

  if (const auto value = readJsonBooleanAfterKey(text, "thermal_enabled")) {
    config.gpu.thermal.enabled = *value;
  }

  if (const auto value = readJsonBooleanAfterKey(text, "thermal_stop_on_critical")) {
    config.gpu.thermal.stopOnCritical = *value;
  }

  if (const auto value = readJsonDoubleAfterKey(text, "thermal_warning_c")) {
    config.gpu.thermal.warningC = *value;
  }

  if (const auto value = readJsonDoubleAfterKey(text, "thermal_critical_c")) {
    config.gpu.thermal.criticalC = *value;
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
         << "    \"engine_id\": \"" << escapeJsonString(config.engine.id) << "\",\n"
         << "    \"backend\": \"" << escapeJsonString(config.engine.backend)
         << "\",\n"
         << "    \"executable\": \"" << escapeJsonString(config.engine.executable)
         << "\"\n"
         << "  },\n"
         << "  \"bitcrack\": {\n"
         << "    \"cuda\": \"" << escapeJsonString(config.bitcrack.cudaPath)
         << "\",\n"
         << "    \"opencl\": \"" << escapeJsonString(config.bitcrack.openclPath)
         << "\"\n"
         << "  },\n"
         << "  \"gpu_device\": " << config.gpu.device << ",\n"
         << "  \"rusticl_enable\": \"" << escapeJsonString(config.gpu.rusticlEnable)
         << "\",\n"
         << "  \"thermal\": {\n"
         << "    \"thermal_enabled\": "
         << (config.gpu.thermal.enabled ? "true" : "false") << ",\n"
         << "    \"thermal_stop_on_critical\": "
         << (config.gpu.thermal.stopOnCritical ? "true" : "false") << ",\n"
         << "    \"thermal_warning_c\": "
         << config.gpu.thermal.warningC << ",\n"
         << "    \"thermal_critical_c\": "
         << config.gpu.thermal.criticalC << "\n"
         << "  },\n"
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
