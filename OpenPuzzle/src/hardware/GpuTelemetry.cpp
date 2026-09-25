#include "openpuzzle/hardware/GpuTelemetry.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

std::string trim(std::string value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }

  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }

  return value;
}

std::string lower(std::string value) {
  std::transform(
      value.begin(),
      value.end(),
      value.begin(),
      [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });
  return value;
}

std::string runCommand(const std::string &command) {
  std::array<char, 512> buffer{};
  std::string output;

  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return output;
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }

  pclose(pipe);
  return output;
}

std::optional<std::string> readText(const fs::path &path) {
  std::ifstream input(path);
  if (!input) {
    return std::nullopt;
  }

  std::string value;
  std::getline(input, value);
  return trim(value);
}

std::optional<double> parseNumber(const std::string &value) {
  const std::string normalized = trim(value);
  if (normalized.empty()) {
    return std::nullopt;
  }

  const std::string normalizedLower = lower(normalized);
  if (normalizedLower == "n/a" ||
      normalizedLower == "[n/a]" ||
      normalizedLower == "not supported" ||
      normalizedLower == "unknown") {
    return std::nullopt;
  }

  try {
    std::size_t consumed = 0;
    const double result = std::stod(normalized, &consumed);

    while (consumed < normalized.size() &&
           std::isspace(
               static_cast<unsigned char>(normalized[consumed]))) {
      ++consumed;
    }

    if (consumed != normalized.size() ||
        !std::isfinite(result)) {
      return std::nullopt;
    }

    return result;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<int> parseNonNegativeInteger(const std::string &value) {
  const auto parsed = parseNumber(value);
  if (!parsed || *parsed < 0.0 ||
      *parsed > static_cast<double>(std::numeric_limits<int>::max()) ||
      std::floor(*parsed) != *parsed) {
    return std::nullopt;
  }

  return static_cast<int>(*parsed);
}

std::optional<double> readScaledValue(
    const fs::path &path,
    const double scale,
    const double minimum,
    const double maximum) {
  const auto text = readText(path);
  if (!text) {
    return std::nullopt;
  }

  const auto raw = parseNumber(*text);
  if (!raw) {
    return std::nullopt;
  }

  const double value = *raw / scale;
  if (value < minimum || value > maximum) {
    return std::nullopt;
  }

  return value;
}

std::vector<std::string> splitCsv(const std::string &line) {
  std::vector<std::string> fields;
  std::stringstream stream(line);
  std::string field;

  while (std::getline(stream, field, ',')) {
    fields.push_back(trim(field));
  }

  return fields;
}

bool cardName(const std::string &name, int &index) {
  if (name.rfind("card", 0) != 0 || name.size() <= 4) {
    return false;
  }

  const std::string suffix = name.substr(4);
  if (!std::all_of(
          suffix.begin(),
          suffix.end(),
          [](const unsigned char character) {
            return std::isdigit(character) != 0;
          })) {
    return false;
  }

  const auto parsed = parseNonNegativeInteger(suffix);
  if (!parsed) {
    return false;
  }

  index = *parsed;
  return true;
}

std::string pciBusId(const fs::path &devicePath) {
  const auto uevent = devicePath / "uevent";
  std::ifstream input(uevent);
  std::string line;

  while (std::getline(input, line)) {
    constexpr const char *prefix = "PCI_SLOT_NAME=";
    if (line.rfind(prefix, 0) == 0) {
      return trim(line.substr(std::char_traits<char>::length(prefix)));
    }
  }

  std::error_code error;
  const auto canonical = fs::canonical(devicePath, error);
  if (!error) {
    return canonical.filename().string();
  }

  return {};
}

std::optional<fs::path> hwmonPath(const fs::path &devicePath) {
  const auto root = devicePath / "hwmon";
  std::error_code error;

  if (!fs::is_directory(root, error)) {
    return std::nullopt;
  }

  for (const auto &entry : fs::directory_iterator(root, error)) {
    if (error) {
      return std::nullopt;
    }

    if (entry.is_directory(error)) {
      return entry.path();
    }
    error.clear();
  }

  return std::nullopt;
}

void readAmdTemperature(
    const fs::path &hwmon,
    GpuTelemetrySnapshot &snapshot) {
  std::error_code error;

  for (const auto &entry : fs::directory_iterator(hwmon, error)) {
    if (error) {
      return;
    }

    const std::string filename = entry.path().filename().string();
    if (filename.rfind("temp", 0) != 0 ||
        filename.size() <= 10 ||
        filename.substr(filename.size() - 6) != "_input") {
      continue;
    }

    const auto value = readScaledValue(
        entry.path(), 1000.0, -100.0, 300.0);
    if (!value ||
        (snapshot.temperatureC && *value <= *snapshot.temperatureC)) {
      continue;
    }

    const std::string sensor = filename.substr(0, filename.size() - 6);
    snapshot.temperatureC = value;
    snapshot.temperatureLabel =
        readText(hwmon / (sensor + "_label")).value_or(sensor);
  }
}

} // namespace

bool GpuTelemetrySnapshot::hasMeasurements() const {
  return temperatureC.has_value() ||
         powerDrawW.has_value() ||
         powerLimitW.has_value();
}

std::vector<GpuTelemetrySnapshot> GpuTelemetry::readAll() {
  auto result = readNvidia();
  auto amd = readAmd();
  result.insert(result.end(), amd.begin(), amd.end());
  return result;
}

std::vector<GpuTelemetrySnapshot> GpuTelemetry::readNvidia() {
  return parseNvidiaSmi(runCommand(
      "nvidia-smi "
      "--query-gpu=index,pci.bus_id,temperature.gpu,power.draw,power.limit "
      "--format=csv,noheader,nounits 2>/dev/null"));
}

std::vector<GpuTelemetrySnapshot> GpuTelemetry::parseNvidiaSmi(
    const std::string &output) {
  std::vector<GpuTelemetrySnapshot> result;
  std::stringstream lines(output);
  std::string line;

  while (std::getline(lines, line)) {
    const auto fields = splitCsv(line);
    if (fields.size() != 5) {
      continue;
    }

    const auto device = parseNonNegativeInteger(fields[0]);
    if (!device) {
      continue;
    }

    GpuTelemetrySnapshot snapshot;
    snapshot.vendor = "NVIDIA";
    snapshot.device = *device;
    snapshot.deviceId = "cuda-" + std::to_string(*device);
    snapshot.pciBusId = fields[1];
    snapshot.temperatureC = parseNumber(fields[2]);
    snapshot.powerDrawW = parseNumber(fields[3]);
    snapshot.powerLimitW = parseNumber(fields[4]);

    if (snapshot.temperatureC &&
        (*snapshot.temperatureC < -100.0 ||
         *snapshot.temperatureC > 300.0)) {
      snapshot.temperatureC.reset();
    }

    if (snapshot.powerDrawW && *snapshot.powerDrawW < 0.0) {
      snapshot.powerDrawW.reset();
    }

    if (snapshot.powerLimitW && *snapshot.powerLimitW < 0.0) {
      snapshot.powerLimitW.reset();
    }

    result.push_back(snapshot);
  }

  return result;
}

std::vector<GpuTelemetrySnapshot> GpuTelemetry::readAmd(
    const fs::path &drmRoot) {
  std::vector<GpuTelemetrySnapshot> result;
  std::error_code error;

  if (!fs::is_directory(drmRoot, error)) {
    return result;
  }

  for (const auto &entry : fs::directory_iterator(drmRoot, error)) {
    if (error) {
      break;
    }

    int device = -1;
    const std::string name = entry.path().filename().string();
    if (!cardName(name, device)) {
      continue;
    }

    const fs::path devicePath = entry.path() / "device";
    const auto vendor = readText(devicePath / "vendor");
    if (!vendor || lower(*vendor) != "0x1002") {
      continue;
    }

    GpuTelemetrySnapshot snapshot;
    snapshot.vendor = "AMD";
    snapshot.device = device;
    snapshot.deviceId = name;
    snapshot.pciBusId = pciBusId(devicePath);

    const auto hwmon = hwmonPath(devicePath);
    if (hwmon) {
      readAmdTemperature(*hwmon, snapshot);

      snapshot.powerDrawW = readScaledValue(
          *hwmon / "power1_average",
          1000000.0,
          0.0,
          10000.0);

      if (!snapshot.powerDrawW) {
        snapshot.powerDrawW = readScaledValue(
            *hwmon / "power1_input",
            1000000.0,
            0.0,
            10000.0);
      }

      snapshot.powerLimitW = readScaledValue(
          *hwmon / "power1_cap",
          1000000.0,
          0.0,
          10000.0);
    }

    result.push_back(snapshot);
  }

  std::sort(
      result.begin(),
      result.end(),
      [](const auto &left, const auto &right) {
        return left.device < right.device;
      });

  return result;
}

} // namespace openpuzzle
