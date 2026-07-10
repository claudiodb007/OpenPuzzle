#include "openpuzzle/hardware/GpuManager.hpp"

#include "openpuzzle/tools/ToolManager.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace openpuzzle {

static std::string runCommand(const char* command) {
  std::array<char, 256> buffer{};
  std::string output;

  FILE* pipe = popen(command, "r");

  if (!pipe) {
    return output;
  }

  while (fgets(buffer.data(), buffer.size(), pipe)) {
    output += buffer.data();
  }

  pclose(pipe);
  return output;
}

static std::string trim(std::string value) {
  while (!value.empty() && value.front() == ' ') {
    value.erase(value.begin());
  }

  while (!value.empty() &&
         (value.back() == ' ' ||
          value.back() == '\n' ||
          value.back() == '\r')) {
    value.pop_back();
  }

  return value;
}

static int parseMemoryMb(const std::string& value) {
  std::stringstream stream(value);

  int memoryMb = 0;
  stream >> memoryMb;

  return memoryMb;
}

std::vector<GpuInfo> GpuManager::listAllGpus() {
  std::vector<GpuInfo> result;

  auto output = runCommand(
      "nvidia-smi "
      "--query-gpu=index,name,uuid,memory.total,driver_version "
      "--format=csv,noheader 2>/dev/null");

  std::stringstream lines(output);
  std::string line;

  while (std::getline(lines, line)) {
    if (line.empty()) {
      continue;
    }

    std::stringstream fields(line);

    std::string index;
    std::string name;
    std::string uuid;
    std::string memory;
    std::string driver;

    std::getline(fields, index, ',');
    std::getline(fields, name, ',');
    std::getline(fields, uuid, ',');
    std::getline(fields, memory, ',');
    std::getline(fields, driver, ',');

    GpuInfo gpu;

    gpu.device = std::stoi(trim(index));
    gpu.name = trim(name);
    gpu.backend = "CUDA";
    gpu.uuid = trim(uuid);
    gpu.driverVersion = trim(driver);
    gpu.memoryMb = parseMemoryMb(trim(memory));
    gpu.cuda = true;
    gpu.opencl = false;

    result.push_back(gpu);
  }

  return result;
}

bool GpuManager::selectGpu(int device) {
  fs::path configPath = ToolManager::configPath();

  fs::create_directories(configPath.parent_path());

  std::string bitcrack;

  if (auto path = ToolManager::bitcrackPath()) {
    bitcrack = *path;
  }

  std::ofstream output(configPath);

  if (!output) {
    return false;
  }

  output
      << "{\n"
      << "  \"bitcrack\": \"" << bitcrack << "\",\n"
      << "  \"gpu_device\": " << device << "\n"
      << "}\n";

  return true;
}

int GpuManager::selectedGpu() {
  std::ifstream input(ToolManager::configPath());

  if (!input) {
    return 0;
  }

  std::stringstream buffer;
  buffer << input.rdbuf();

  std::string text = buffer.str();

  auto key = text.find("\"gpu_device\"");

  if (key == std::string::npos) {
    return 0;
  }

  auto colon = text.find(':', key);

  if (colon == std::string::npos) {
    return 0;
  }

  return std::stoi(text.substr(colon + 1));
}

GpuInfo GpuManager::currentGpu() {
  const int selected = selectedGpu();

  for (const auto& gpu : listGpus()) {
    if (gpu.device == selected) {
      return gpu;
    }
  }

  GpuInfo fallback;

  fallback.device = selected;
  fallback.name = "GPU " + std::to_string(selected);
  fallback.backend = "CUDA";
  fallback.cuda = true;

  return fallback;
}

} // namespace openpuzzle
