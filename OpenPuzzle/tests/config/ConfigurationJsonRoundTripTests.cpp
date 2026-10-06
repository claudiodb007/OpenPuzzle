#include "openpuzzle/config/ConfigurationManager.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

int main() {
  const char* previous = std::getenv("HOME");
  const std::string previousHome = previous ? previous : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-config-json-" + std::to_string(getpid()));
  fs::remove_all(root); fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);
  openpuzzle::Configuration configuration;
  configuration.engine.id = "bitcrack";
  configuration.engine.backend = "opencl";
  configuration.engine.executable = "/tmp/engine \"quoted\"/back\\slash";
  configuration.bitcrack.cudaPath = "/tmp/é/space folder/cuBitCrack";
  configuration.bitcrack.openclPath = "/tmp/\"cuda\"/clBitCrack";
  configuration.gpu.rusticlEnable = "radeonsi";
  configuration.gpu.device = 2;
  configuration.gpu.thermal.enabled = true;
  configuration.gpu.thermal.warningC = 73.5;
  configuration.assignment.durationMinutes = 7;
  assert(openpuzzle::ConfigurationManager::save(configuration));
  const auto loaded = openpuzzle::ConfigurationManager::load();
  std::cout << "Configured executable retained: "
            << (loaded.engine.executable == configuration.engine.executable) << std::endl;
  assert(loaded.engine.executable == configuration.engine.executable);
  assert(loaded.bitcrack.cudaPath == configuration.bitcrack.cudaPath);
  assert(loaded.bitcrack.openclPath == configuration.bitcrack.openclPath);
  assert(loaded.engine.backend == configuration.engine.backend);
  assert(loaded.gpu.device == 2 && loaded.gpu.rusticlEnable == "radeonsi");
  assert(loaded.gpu.thermal.enabled && loaded.gpu.thermal.warningC == 73.5);
  assert(loaded.assignment.durationMinutes == 7);

  configuration.engine.executable += "\n\t\r\b\f";
  configuration.bitcrack.openclPath += std::string(1, '\x01');
  assert(openpuzzle::ConfigurationManager::save(configuration));
  const auto controls = openpuzzle::ConfigurationManager::load();
  assert(controls.engine.executable == configuration.engine.executable);
  assert(controls.bitcrack.openclPath == configuration.bitcrack.openclPath);

  // Existing flat legacy configuration and escaped Unicode remain readable.
  {
    std::ofstream file(openpuzzle::ConfigurationManager::configPath());
    file << R"({"bitcrack":"/tmp/legacy\\tools/cuBitCrack","engine_id":"bitcrack","backend":"cuda","executable":"/tmp/\u00e9/engine","gpu_device":1})";
  }
  const auto legacy = openpuzzle::ConfigurationManager::load();
  assert(legacy.bitcrack.cudaPath == "/tmp/legacy\\tools/cuBitCrack");
  assert(legacy.engine.executable == "/tmp/é/engine");
  assert(legacy.gpu.device == 1);
  if (previous) assert(setenv("HOME", previousHome.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  fs::remove_all(root);
  std::cout << "ConfigurationJsonRoundTripTests passed\n";
}
