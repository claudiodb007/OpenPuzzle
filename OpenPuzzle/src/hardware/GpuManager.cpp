#include "openpuzzle/hardware/GpuManager.hpp"

#include "openpuzzle/tools/ToolManager.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

std::string runCommand(
    const std::string &command) {
  std::array<char, 512> buffer{};
  std::string output;

  FILE *pipe =
      popen(
          command.c_str(),
          "r");

  if (!pipe) {
    return output;
  }

  while (
      fgets(
          buffer.data(),
          buffer.size(),
          pipe)) {
    output += buffer.data();
  }

  pclose(pipe);

  return output;
}

std::string trim(
    std::string value) {
  while (
      !value.empty() &&
      std::isspace(
          static_cast<unsigned char>(
              value.front()))) {
    value.erase(
        value.begin());
  }

  while (
      !value.empty() &&
      std::isspace(
          static_cast<unsigned char>(
              value.back()))) {
    value.pop_back();
  }

  return value;
}

std::string shellQuote(
    const std::string &value) {
  std::string result = "'";

  for (const char character : value) {
    if (character == '\'') {
      result += "'\\''";
    } else {
      result += character;
    }
  }

  result += '\'';

  return result;
}

std::optional<int> parseInteger(
    const std::string &value) {
  std::size_t position = 0;

  while (
      position < value.size() &&
      !std::isdigit(
          static_cast<unsigned char>(
              value[position])) &&
      value[position] != '-') {
    ++position;
  }

  if (position >= value.size()) {
    return std::nullopt;
  }

  try {
    std::size_t consumed = 0;

    const int result =
        std::stoi(
            value.substr(position),
            &consumed);

    if (consumed == 0) {
      return std::nullopt;
    }

    return result;
  } catch (...) {
    return std::nullopt;
  }
}

int parseMemoryMb(
    const std::string &value) {
  const auto parsed =
      parseInteger(value);

  return parsed.value_or(0);
}

std::vector<GpuInfo> discoverBitCrackDevices(
    const std::optional<std::string> &executable,
    const std::string &backend) {
  if (!executable ||
      executable->empty() ||
      !fs::is_regular_file(*executable)) {
    return {};
  }

  const std::string output =
      runCommand(
          shellQuote(*executable) +
          " --list-devices 2>/dev/null");

  return GpuManager::parseBitCrackDevices(
      output,
      backend);
}

} // namespace

std::vector<GpuInfo>
GpuManager::parseBitCrackDevices(
    const std::string &output,
    const std::string &backend) {
  std::vector<GpuInfo> result;

  std::stringstream lines(output);
  std::string line;

  GpuInfo current;
  bool active = false;

  const auto beginDevice =
      [&]() {
        current = GpuInfo{};
        current.backend = backend;
        current.cuda =
            backend == "CUDA";
        current.opencl =
            backend == "OpenCL";
        active = true;
      };

  const auto finishDevice =
      [&]() {
        if (!active ||
            current.name.empty()) {
          return;
        }

        result.push_back(current);
        active = false;
      };

  while (std::getline(
      lines,
      line)) {
    line = trim(line);

    if (line.empty()) {
      continue;
    }

    const auto separator =
        line.find(':');

    if (separator ==
        std::string::npos) {
      continue;
    }

    const std::string label =
        trim(
            line.substr(
                0,
                separator));

    const std::string value =
        trim(
            line.substr(
                separator + 1));

    if (label == "ID") {
      finishDevice();
      beginDevice();

      current.device =
          parseInteger(value)
              .value_or(0);

      continue;
    }

    if (!active) {
      continue;
    }

    if (label == "Name") {
      current.name = value;
    } else if (label == "Memory") {
      current.memoryMb =
          parseMemoryMb(value);
    } else if (
        label == "Compute units" ||
        label == "Compute Units") {
      current.computeUnits =
          parseInteger(value)
              .value_or(0);
    }
  }

  finishDevice();

  return result;
}

std::vector<GpuInfo>
GpuManager::listCudaGpus() {
  std::vector<GpuInfo> result;

  const std::string output =
      runCommand(
          "nvidia-smi "
          "--query-gpu="
          "index,name,uuid,memory.total,"
          "driver_version "
          "--format=csv,noheader "
          "2>/dev/null");

  std::stringstream lines(output);
  std::string line;

  while (std::getline(
      lines,
      line)) {
    if (line.empty()) {
      continue;
    }

    std::stringstream fields(line);

    std::string index;
    std::string name;
    std::string uuid;
    std::string memory;
    std::string driver;

    std::getline(
        fields,
        index,
        ',');

    std::getline(
        fields,
        name,
        ',');

    std::getline(
        fields,
        uuid,
        ',');

    std::getline(
        fields,
        memory,
        ',');

    std::getline(
        fields,
        driver,
        ',');

    GpuInfo gpu;

    try {
      gpu.device =
          std::stoi(
              trim(index));
    } catch (...) {
      continue;
    }

    gpu.name = trim(name);
    gpu.backend = "CUDA";
    gpu.uuid = trim(uuid);
    gpu.driverVersion = trim(driver);
    gpu.memoryMb =
        parseMemoryMb(
            trim(memory));
    gpu.cuda = true;
    gpu.opencl = false;

    result.push_back(gpu);
  }

  const auto bitcrackDevices =
      discoverBitCrackDevices(
          ToolManager::bitcrackCudaPath(),
          "CUDA");

  if (result.empty()) {
    return bitcrackDevices;
  }

  for (auto &gpu : result) {
    for (const auto &device :
         bitcrackDevices) {
      if (device.device !=
          gpu.device) {
        continue;
      }

      gpu.computeUnits =
          device.computeUnits;

      if (gpu.name.empty()) {
        gpu.name = device.name;
      }

      if (gpu.memoryMb <= 0) {
        gpu.memoryMb =
            device.memoryMb;
      }

      break;
    }
  }

  return result;
}

std::vector<GpuInfo>
GpuManager::listOpenClGpus() {
  return discoverBitCrackDevices(
      ToolManager::bitcrackOpenCLPath(),
      "OpenCL");
}

std::vector<GpuInfo>
GpuManager::listGpus() {
  return listCudaGpus();
}

std::vector<GpuInfo>
GpuManager::listAllGpus() {
  auto result =
      listCudaGpus();

  auto opencl =
      listOpenClGpus();

  result.insert(
      result.end(),
      opencl.begin(),
      opencl.end());

  return result;
}

bool GpuManager::selectGpu(
    int device) {
  fs::path configPath =
      ToolManager::configPath();

  fs::create_directories(
      configPath.parent_path());

  std::string bitcrack;

  if (const auto path =
          ToolManager::bitcrackPath()) {
    bitcrack = *path;
  }

  std::ofstream output(
      configPath);

  if (!output) {
    return false;
  }

  output
      << "{\n"
      << "  \"bitcrack\": \""
      << bitcrack
      << "\",\n"
      << "  \"gpu_device\": "
      << device
      << "\n"
      << "}\n";

  return true;
}

int GpuManager::selectedGpu() {
  std::ifstream input(
      ToolManager::configPath());

  if (!input) {
    return 0;
  }

  std::stringstream buffer;
  buffer << input.rdbuf();

  const std::string text =
      buffer.str();

  const auto key =
      text.find(
          "\"gpu_device\"");

  if (key ==
      std::string::npos) {
    return 0;
  }

  const auto colon =
      text.find(
          ':',
          key);

  if (colon ==
      std::string::npos) {
    return 0;
  }

  try {
    return std::stoi(
        text.substr(
            colon + 1));
  } catch (...) {
    return 0;
  }
}

GpuInfo GpuManager::currentGpu() {
  return currentGpu(
      "CUDA");
}

GpuInfo GpuManager::currentGpu(
    const std::string &backend) {
  const int selected =
      selectedGpu();

  for (const auto &gpu :
       listAllGpus()) {
    if (
        gpu.device == selected &&
        gpu.backend == backend) {
      return gpu;
    }
  }

  GpuInfo fallback;

  fallback.device = selected;
  fallback.name =
      "GPU " +
      std::to_string(selected);
  fallback.backend = backend;
  fallback.cuda =
      backend == "CUDA";
  fallback.opencl =
      backend == "OpenCL";

  return fallback;
}

} // namespace openpuzzle
