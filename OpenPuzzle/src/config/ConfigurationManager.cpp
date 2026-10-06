#include "openpuzzle/config/ConfigurationManager.hpp"

#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/JsonString.hpp"
#include "openpuzzle/core/JsonNumber.hpp"
#include "openpuzzle/core/AtomicFile.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cerrno>
#include <cstdlib>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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

std::optional<int> readJsonIntegerAfterKey(
    const boost::property_tree::ptree& document,
    const std::string& key) {
  const auto text = findJsonString(document, key);
  if (!text) {
    return std::nullopt;
  }
  int value = 0;
  const auto parsed = std::from_chars(
      text->data(), text->data() + text->size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size()) {
    return std::nullopt;
  }
  return value;
}

std::optional<double> readJsonDoubleAfterKey(
    const boost::property_tree::ptree& document,
    const std::string& key) {
  const auto text = findJsonString(document, key);
  if (!text) {
    return std::nullopt;
  }
  std::istringstream input(*text);
  input.imbue(std::locale::classic());
  double value = 0.0;
  if (!(input >> value) || !std::isfinite(value)) {
    return std::nullopt;
  }
  input >> std::ws;
  if (!input.eof()) {
    return std::nullopt;
  }
  return value;
}

std::optional<bool> readJsonBooleanAfterKey(
    const boost::property_tree::ptree& document,
    const std::string& key) {
  const auto text = findJsonString(document, key);
  if (text == "true") {
    return true;
  }
  if (text == "false") {
    return false;
  }
  return std::nullopt;
}

std::optional<std::string> readFile(
    const std::string& path, bool& missing) {
  missing = false;
  const int descriptor = ::open(
      path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (descriptor < 0) {
    missing = errno == ENOENT;
    return std::nullopt;
  }
  struct Input {
    int descriptor;
    ~Input() { ::close(descriptor); }
  } input{descriptor};
  struct stat metadata {};
  if (::fstat(descriptor, &metadata) != 0 || !S_ISREG(metadata.st_mode)) {
    return std::nullopt;
  }
  // Protect the file that was opened, without following a changed path.
  if (::fchmod(descriptor, S_IRUSR | S_IWUSR) != 0) {
    return std::nullopt;
  }
  std::string contents;
  char buffer[4096];
  for (;;) {
    const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) return std::nullopt;
    if (count == 0) break;
    contents.append(buffer, static_cast<std::size_t>(count));
  }
  return contents;
}

} // namespace

std::string ConfigurationManager::configPath() {
  const char *home = std::getenv("HOME");

  fs::path path = home ? fs::path(home) : fs::current_path();

  path /= ".config/OpenPuzzle/config.json";

  return path.string();
}

Configuration ConfigurationManager::load() {
  return loadChecked().value_or(Configuration{});
}

std::optional<Configuration> ConfigurationManager::loadChecked() {
  Configuration config;
  const fs::path path = configPath();
  boost::property_tree::ptree document;
  try {
    WorkspaceSecurity::prepare(path.parent_path());
    bool missing = false;
    const auto text = readFile(path.string(), missing);
    if (missing) {
      return config;
    }
    if (!text) {
      return std::nullopt;
    }
    // Configuration is a JSON object. An existing empty file, array or scalar
    // must not be mistaken for missing settings during an update.
    const auto first = text->find_first_not_of(" \t\r\n");
    if (first == std::string::npos || (*text)[first] != '{') {
      return std::nullopt;
    }
    std::istringstream input(*text);
    boost::property_tree::read_json(input, document);
  } catch (...) {
    return std::nullopt;
  }

  if (const auto value = findJsonString(document, "cuda")) {
    config.bitcrack.cudaPath = *value;
  } else if (const auto legacy = findJsonString(document, "bitcrack")) {
    config.bitcrack.cudaPath = *legacy;
  }

  if (const auto value = findJsonString(document, "opencl")) {
    config.bitcrack.openclPath = *value;
  } else if (!config.bitcrack.cudaPath.empty()) {
    config.bitcrack.openclPath =
        (fs::path(config.bitcrack.cudaPath).parent_path() / "clBitCrack")
            .string();
  }

  if (const auto value = findJsonString(document, "engine_id")) {
    config.engine.id = *value;
  }

  if (const auto value = findJsonString(document, "backend")) {
    config.engine.backend = *value;
  }

  if (const auto value = findJsonString(document, "executable")) {
    config.engine.executable = *value;
  }

  if (const auto value = readJsonIntegerAfterKey(document, "gpu_device")) {
    config.gpu.device = *value;
  }

  if (const auto value = findJsonString(document, "rusticl_enable")) {
    config.gpu.rusticlEnable = *value;
  }

  if (const auto value = readJsonBooleanAfterKey(document, "thermal_enabled")) {
    config.gpu.thermal.enabled = *value;
  }

  if (const auto value = readJsonBooleanAfterKey(document, "thermal_stop_on_critical")) {
    config.gpu.thermal.stopOnCritical = *value;
  }

  if (const auto value = readJsonDoubleAfterKey(document, "thermal_warning_c")) {
    config.gpu.thermal.warningC = *value;
  }

  if (const auto value = readJsonDoubleAfterKey(document, "thermal_critical_c")) {
    config.gpu.thermal.criticalC = *value;
  }

  if (const auto value = readJsonIntegerAfterKey(document, "duration_minutes")) {
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

  if (!std::isfinite(config.gpu.thermal.warningC) ||
      !std::isfinite(config.gpu.thermal.criticalC)) {
    return false;
  }

  std::ostringstream output;
  output.imbue(std::locale::classic());

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
         << encodeJsonDouble(config.gpu.thermal.warningC) << ",\n"
         << "    \"thermal_critical_c\": "
         << encodeJsonDouble(config.gpu.thermal.criticalC) << "\n"
         << "  },\n"
         << "  \"assignment\": {\n"
         << "    \"duration_minutes\": " << config.assignment.durationMinutes
         << "\n"
         << "  }\n"
         << "}\n";

  return writePrivateFileAtomically(path, output.str());
}

} // namespace openpuzzle
