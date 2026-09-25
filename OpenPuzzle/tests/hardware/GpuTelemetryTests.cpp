#include "openpuzzle/hardware/GpuTelemetry.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

using namespace openpuzzle;

namespace {

bool near(const double left, const double right) {
  const double difference = left > right ? left - right : right - left;
  return difference < 0.001;
}

void write(const fs::path &path, const std::string &value) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path);
  output << value;
}

fs::path temporaryDirectory() {
  const auto suffix = std::chrono::steady_clock::now()
                          .time_since_epoch()
                          .count();
  const fs::path path = fs::temp_directory_path() /
      ("openpuzzle-gpu-telemetry-" + std::to_string(suffix));
  fs::create_directories(path);
  return path;
}

} // namespace

int main() {
  const auto nvidia = GpuTelemetry::parseNvidiaSmi(
      "0, 00000000:01:00.0, 61, 148.75, 250.00\n"
      "1, 00000000:02:00.0, [N/A], N/A, 290.00\n"
      "invalid, 00000000:03:00.0, 50, 100, 200\n"
      "2, incomplete\n");

  if (nvidia.size() != 2) {
    return 1;
  }

  if (nvidia[0].vendor != "NVIDIA" ||
      nvidia[0].device != 0 ||
      nvidia[0].deviceId != "cuda-0" ||
      nvidia[0].pciBusId != "00000000:01:00.0") {
    return 2;
  }

  if (!nvidia[0].temperatureC ||
      !near(*nvidia[0].temperatureC, 61.0) ||
      !nvidia[0].powerDrawW ||
      !near(*nvidia[0].powerDrawW, 148.75) ||
      !nvidia[0].powerLimitW ||
      !near(*nvidia[0].powerLimitW, 250.0) ||
      !nvidia[0].hasMeasurements()) {
    return 3;
  }

  if (nvidia[1].temperatureC ||
      nvidia[1].powerDrawW ||
      !nvidia[1].powerLimitW ||
      !near(*nvidia[1].powerLimitW, 290.0)) {
    return 4;
  }

  const fs::path root = temporaryDirectory();
  const fs::path amd = root / "card3" / "device";
  const fs::path hwmon = amd / "hwmon" / "hwmon7";

  write(amd / "vendor", "0x1002\n");
  write(amd / "uevent", "DRIVER=amdgpu\nPCI_SLOT_NAME=0000:08:00.0\n");
  write(hwmon / "temp1_input", "62500\n");
  write(hwmon / "temp1_label", "edge\n");
  write(hwmon / "temp2_input", "81000\n");
  write(hwmon / "temp2_label", "junction\n");
  write(hwmon / "power1_average", "148500000\n");
  write(hwmon / "power1_cap", "250000000\n");

  write(root / "card4" / "device" / "vendor", "0x10de\n");
  write(root / "card5" / "device" / "vendor", "0x1002\n");

  const auto amdReadings = GpuTelemetry::readAmd(root);
  fs::remove_all(root);

  if (amdReadings.size() != 2) {
    return 5;
  }

  const auto &measured = amdReadings[0];
  if (measured.vendor != "AMD" ||
      measured.device != 3 ||
      measured.deviceId != "card3" ||
      measured.pciBusId != "0000:08:00.0") {
    return 6;
  }

  if (!measured.temperatureC ||
      !near(*measured.temperatureC, 81.0) ||
      measured.temperatureLabel != "junction" ||
      !measured.powerDrawW ||
      !near(*measured.powerDrawW, 148.5) ||
      !measured.powerLimitW ||
      !near(*measured.powerLimitW, 250.0)) {
    return 7;
  }

  if (amdReadings[1].device != 5 ||
      amdReadings[1].hasMeasurements()) {
    return 8;
  }

  const auto missing = GpuTelemetry::readAmd(root / "missing");
  if (!missing.empty()) {
    return 9;
  }

  return 0;
}
