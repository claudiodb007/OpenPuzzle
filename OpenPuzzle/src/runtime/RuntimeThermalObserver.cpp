#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <ostream>
#include <sstream>
#include <utility>

namespace openpuzzle {

namespace {

std::string gpuVendor(std::string name) {
  std::transform(
      name.begin(),
      name.end(),
      name.begin(),
      [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });

  if (
      name.find("nvidia") != std::string::npos ||
      name.find("geforce") != std::string::npos ||
      name.find("quadro") != std::string::npos ||
      name.find("tesla") != std::string::npos) {
    return "NVIDIA";
  }

  if (
      name.find("amd") != std::string::npos ||
      name.find("radeon") != std::string::npos ||
      name.find("advanced micro devices") != std::string::npos) {
    return "AMD";
  }

  return {};
}

} // namespace

RuntimeThermalObserver::RuntimeThermalObserver(
    GpuThermalPolicyConfiguration policy,
    Reader reader,
    const std::chrono::seconds sampleInterval,
    const std::chrono::seconds reminderInterval)
    : policy_(std::move(policy)),
      reader_(std::move(reader)),
      sampleInterval_(sampleInterval),
      reminderInterval_(reminderInterval) {}

RuntimeThermalObserver::RuntimeThermalObserver(
    GpuThermalPolicyConfiguration policy,
    DeviceScope deviceScope,
    Reader reader,
    const std::chrono::seconds sampleInterval,
    const std::chrono::seconds reminderInterval)
    : policy_(std::move(policy)),
      reader_(std::move(reader)),
      sampleInterval_(sampleInterval),
      reminderInterval_(reminderInterval) {
  if (deviceScope) {
    deviceScope_.emplace(
        deviceScope->begin(),
        deviceScope->end());
  }
}

RuntimeThermalObserver::DeviceScope
RuntimeThermalObserver::executionScope(
    std::string backend,
    const int device) {
  std::transform(
      backend.begin(),
      backend.end(),
      backend.begin(),
      [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });

  if (backend == "cuda" && device >= 0) {
    return std::vector<std::string>{
        "cuda-" + std::to_string(device)};
  }

  if (backend == "cpu") {
    return std::vector<std::string>{};
  }

  /*
   * OpenCL indexes are backend-local and cannot be mapped safely to a DRM
   * card or NVIDIA index on mixed-vendor hosts. Keep conservative whole-host
   * monitoring until the OpenCL inventory exposes a stable physical ID.
   */
  return std::nullopt;
}

RuntimeThermalObserver::DeviceScope
RuntimeThermalObserver::cudaScope(
    const std::vector<int> &devices) {
  std::vector<std::string> result;
  result.reserve(devices.size());

  for (const int device : devices) {
    if (device >= 0) {
      result.push_back(
          "cuda-" + std::to_string(device));
    }
  }

  return result;
}

RuntimeThermalObserver::DeviceScope
RuntimeThermalObserver::openclScope(
    const std::vector<GpuInfo> &availableDevices,
    const std::vector<int> &selectedDevices,
    const std::vector<GpuTelemetrySnapshot> &telemetry) {
  std::map<std::string, std::size_t> availableByVendor;
  std::map<std::string, std::size_t> selectedByVendor;
  std::map<std::string, std::vector<std::string>> telemetryByVendor;

  for (const auto &gpu : availableDevices) {
    const std::string vendor = gpuVendor(gpu.name);
    if (!vendor.empty()) {
      ++availableByVendor[vendor];
    }
  }

  for (const int selectedDevice : selectedDevices) {
    const auto found = std::find_if(
        availableDevices.begin(),
        availableDevices.end(),
        [selectedDevice](const GpuInfo &gpu) {
          return gpu.device == selectedDevice;
        });

    if (found == availableDevices.end()) {
      return std::nullopt;
    }

    const std::string vendor = gpuVendor(found->name);
    if (vendor.empty()) {
      return std::nullopt;
    }

    ++selectedByVendor[vendor];
  }

  for (const auto &snapshot : telemetry) {
    if (
        !snapshot.vendor.empty() &&
        !snapshot.deviceId.empty()) {
      telemetryByVendor[snapshot.vendor].push_back(
          snapshot.deviceId);
    }
  }

  std::vector<std::string> result;

  for (const auto &[vendor, selectedCount] : selectedByVendor) {
    const auto available = availableByVendor.find(vendor);
    const auto readings = telemetryByVendor.find(vendor);

    if (
        available == availableByVendor.end() ||
        readings == telemetryByVendor.end() ||
        readings->second.empty()) {
      return std::nullopt;
    }

    const bool uniquePhysicalDevice =
        available->second == 1 &&
        selectedCount == 1 &&
        readings->second.size() == 1;

    const bool completeVendorSelection =
        selectedCount == available->second &&
        readings->second.size() == available->second;

    if (!uniquePhysicalDevice && !completeVendorSelection) {
      return std::nullopt;
    }

    result.insert(
        result.end(),
        readings->second.begin(),
        readings->second.end());
  }

  return result;
}

RuntimeThermalObserver::DeviceScope
RuntimeThermalObserver::combineScopes(
    const DeviceScope &first,
    const DeviceScope &second) {
  if (!first || !second) {
    return std::nullopt;
  }

  std::vector<std::string> result = *first;
  result.insert(
      result.end(),
      second->begin(),
      second->end());

  std::sort(result.begin(), result.end());
  result.erase(
      std::unique(result.begin(), result.end()),
      result.end());

  return result;
}

bool RuntimeThermalObserver::enabled() const {
  return processOwnsMonitoring() &&
         policy_.enabled &&
         GpuThermalPolicy::valid(policy_) &&
         static_cast<bool>(reader_) &&
         (!deviceScope_ || !deviceScope_->empty()) &&
         sampleInterval_.count() > 0 &&
         reminderInterval_.count() > 0;
}

bool RuntimeThermalObserver::protectionEnabled() const {
  return enabled() && policy_.stopOnCritical;
}

std::vector<RuntimeThermalEvent>
RuntimeThermalObserver::poll() {
  return pollAt(Clock::now());
}

std::vector<RuntimeThermalEvent>
RuntimeThermalObserver::pollAt(
    const Clock::time_point now) {
  std::vector<RuntimeThermalEvent> events;

  if (!enabled() ||
      (nextSample_ && now < *nextSample_)) {
    return events;
  }

  nextSample_ = now + sampleInterval_;

  for (const auto &snapshot : reader_()) {
    if (
        deviceScope_ &&
        deviceScope_->find(snapshot.deviceId) ==
            deviceScope_->end()) {
      continue;
    }

    const auto current =
        GpuThermalPolicy::evaluate(
            policy_, snapshot.temperatureC);

    auto &device = states_[snapshot.deviceId];
    const auto previous = device.state;

    const bool hot =
        current == GpuThermalState::Warning ||
        current == GpuThermalState::Critical;

    const bool wasHot =
        previous == GpuThermalState::Warning ||
        previous == GpuThermalState::Critical;

    if (hot) {
      const bool transition = current != previous;
      const bool reminder =
          !transition &&
          device.lastNotice &&
          now - *device.lastNotice >= reminderInterval_;

      if (transition || reminder) {
        RuntimeThermalEvent event;
        event.kind =
            current == GpuThermalState::Critical
                ? RuntimeThermalEventKind::Critical
                : RuntimeThermalEventKind::Warning;
        event.snapshot = snapshot;
        event.thresholdC =
            current == GpuThermalState::Critical
                ? policy_.criticalC
                : policy_.warningC;
        event.reminder = reminder;
        events.push_back(std::move(event));
        device.lastNotice = now;
      }
    } else if (
        current == GpuThermalState::Normal &&
        wasHot) {
      RuntimeThermalEvent event;
      event.kind = RuntimeThermalEventKind::Recovered;
      event.snapshot = snapshot;
      events.push_back(std::move(event));
      device.lastNotice.reset();
    }

    device.state = current;
  }

  return events;
}

bool RuntimeThermalObserver::processOwnsMonitoring() {
  const char *value = std::getenv(OwnerEnvironment);
  return value == nullptr || std::string(value) != "0";
}

void RuntimeThermalObserver::print(
    const RuntimeThermalEvent &event,
    std::ostream &output) {
  const bool recovered =
      event.kind == RuntimeThermalEventKind::Recovered;
  const bool critical =
      event.kind == RuntimeThermalEventKind::Critical;

  std::ostringstream message;

  message
      << '\n'
      << "OpenPuzzle thermal "
      << (recovered ? "recovery" : "warning")
      << '\n'
      << "--------------------------\n"
      << "Device.............. "
      << event.snapshot.vendor << ' '
      << event.snapshot.deviceId << '\n'
      << std::fixed << std::setprecision(1);

  if (event.snapshot.temperatureC) {
    message
        << "Temperature......... "
        << *event.snapshot.temperatureC
        << " C\n";
  } else {
    message
        << "Temperature......... unavailable\n";
  }

  message
      << "State............... "
      << (recovered
              ? "NORMAL"
              : (critical ? "CRITICAL" : "WARNING"));

  if (event.reminder) {
    message << " (reminder)";
  }

  message << '\n';

  if (!recovered) {
    message
        << "Threshold........... "
        << event.thresholdC
        << " C\n";
  }

  message
      << "Action.............. "
      << (recovered
              ? "monitoring continues"
              : "diagnostic only; execution continues")
      << '\n';

  output << message.str();
}

} // namespace openpuzzle
