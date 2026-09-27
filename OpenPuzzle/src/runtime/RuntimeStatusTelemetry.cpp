#include "openpuzzle/runtime/RuntimeStatusTelemetry.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace openpuzzle {

namespace {

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

std::string gpuVendor(const std::string &name) {
  const std::string normalized = lower(name);

  if (
      normalized.find("nvidia") != std::string::npos ||
      normalized.find("geforce") != std::string::npos ||
      normalized.find("quadro") != std::string::npos ||
      normalized.find("tesla") != std::string::npos) {
    return "NVIDIA";
  }

  if (
      normalized.find("amd") != std::string::npos ||
      normalized.find("radeon") != std::string::npos ||
      normalized.find("advanced micro devices") != std::string::npos) {
    return "AMD";
  }

  return {};
}

std::string fixed(const double value) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(1) << value;
  return output.str();
}

} // namespace

std::optional<GpuTelemetrySnapshot>
RuntimeStatusTelemetry::select(
    std::string backend,
    const int device,
    const std::string &gpuName,
    const std::vector<GpuTelemetrySnapshot> &snapshots) {
  backend = lower(std::move(backend));

  if (backend == "cuda" && device >= 0) {
    const auto found = std::find_if(
        snapshots.begin(),
        snapshots.end(),
        [device](const GpuTelemetrySnapshot &snapshot) {
          return snapshot.vendor == "NVIDIA" &&
                 snapshot.device == device;
        });

    if (found != snapshots.end()) {
      return *found;
    }
    return std::nullopt;
  }

  if (backend != "opencl") {
    return std::nullopt;
  }

  const std::string vendor = gpuVendor(gpuName);
  if (vendor.empty()) {
    return std::nullopt;
  }

  const GpuTelemetrySnapshot *match = nullptr;
  for (const auto &snapshot : snapshots) {
    if (
        snapshot.vendor != vendor ||
        !snapshot.hasMeasurements()) {
      continue;
    }

    if (match != nullptr) {
      return std::nullopt;
    }
    match = &snapshot;
  }

  return match == nullptr
      ? std::nullopt
      : std::optional<GpuTelemetrySnapshot>(*match);
}

std::string RuntimeStatusTelemetry::temperatureText(
    const GpuTelemetrySnapshot &snapshot) {
  if (!snapshot.temperatureC) {
    return {};
  }

  std::string result = fixed(*snapshot.temperatureC) + " C";
  if (!snapshot.temperatureLabel.empty()) {
    result += " (" + snapshot.temperatureLabel + ")";
  }
  return result;
}

std::string RuntimeStatusTelemetry::powerText(
    const GpuTelemetrySnapshot &snapshot) {
  if (!snapshot.powerDrawW && !snapshot.powerLimitW) {
    return {};
  }

  if (snapshot.powerDrawW && snapshot.powerLimitW) {
    return fixed(*snapshot.powerDrawW) + " W / " +
           fixed(*snapshot.powerLimitW) + " W";
  }

  if (snapshot.powerDrawW) {
    return fixed(*snapshot.powerDrawW) + " W";
  }

  return "limit " + fixed(*snapshot.powerLimitW) + " W";
}

} // namespace openpuzzle
