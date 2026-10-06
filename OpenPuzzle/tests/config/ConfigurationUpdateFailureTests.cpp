#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/core/commands/BenchmarkCommand.hpp"
#include "openpuzzle/core/commands/ThermalCommand.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace openpuzzle;

namespace {
std::string read(const fs::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}

void write(const fs::path& path, const std::string& text) {
  std::ofstream output(path);
  output << text;
  assert(output.good());
}

int gpuSelect(int device) {
  Application app;
  std::string command = "openpuzzle", action = "gpu-select";
  std::string flag = "--device", value = std::to_string(device);
  char* argv[] = {command.data(), action.data(), flag.data(), value.data()};
  return app.run(4, argv);
}
}

int main() {
  const char* oldHome = getenv("HOME");
  const std::string savedHome = oldHome ? oldHome : "";
  const char* oldRusticl = getenv("RUSTICL_ENABLE");
  const std::string savedRusticl = oldRusticl ? oldRusticl : "";
  const auto root = fs::temp_directory_path() /
      ("openpuzzle-config-update-" + std::to_string(getpid()));
  fs::remove_all(root);
  fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);

  Configuration initial;
  initial.engine.id = "kangaroo";
  initial.engine.backend = "opencl";
  initial.engine.executable = "/tmp/quoted \"engine\"";
  initial.bitcrack.cudaPath = "/tmp/custom\\cuda";
  initial.bitcrack.openclPath = "/tmp/custom\\opencl";
  initial.gpu.thermal.enabled = true;
  initial.gpu.thermal.stopOnCritical = true;
  initial.gpu.thermal.warningC = 71.5;
  initial.gpu.thermal.criticalC = 84.5;
  initial.gpu.rusticlEnable = "radeonsi";
  initial.assignment.durationMinutes = 42;
  assert(ConfigurationManager::save(initial));
  const fs::path path = ConfigurationManager::configPath();

  assert(gpuSelect(3) == 0);
  const auto selected = ConfigurationManager::load();
  assert(selected.gpu.device == 3);
  assert(selected.engine.id == initial.engine.id);
  assert(selected.engine.backend == initial.engine.backend);
  assert(selected.engine.executable == initial.engine.executable);
  assert(selected.bitcrack.cudaPath == initial.bitcrack.cudaPath);
  assert(selected.bitcrack.openclPath == initial.bitcrack.openclPath);
  assert(selected.gpu.thermal.enabled && selected.gpu.thermal.stopOnCritical);
  assert(selected.gpu.thermal.warningC == 71.5 && selected.gpu.thermal.criticalC == 84.5);
  assert(selected.gpu.rusticlEnable == "radeonsi");
  assert(selected.assignment.durationMinutes == 42);
  const auto intact = read(path);
  assert(!GpuManager::selectGpu(-1));
  assert(gpuSelect(-1) != 0 && read(path) == intact);

  // Invalid requests stop before device discovery, database setup or mutation.
  assert(setenv("RUSTICL_ENABLE", "original-selector", 1) == 0);
  for (const auto& args : std::vector<std::vector<std::string>>{
           {"--seconds", "1"}, {"--samples", "3"}, {"--blocks", "0"},
           {"--threads", "0"}, {"--points", "0"}}) {
    auto request = args;
    request.insert(request.end(), {"--backend", "opencl", "--rusticl-enable", "iris"});
    bool failed = false;
    try { BenchmarkCommand{}.run(request); }
    catch (const std::exception&) { failed = true; }
    assert(failed && read(path) == intact);
    assert(std::string(getenv("RUSTICL_ENABLE")) == "original-selector");
    assert(!fs::exists(root / ".local/share/OpenPuzzle/openpuzzle.db"));
  }

  for (const auto& broken : {
           std::string{}, std::string("{\"gpu_device\":7,"),
           std::string(R"([{"gpu_device":7}])")}) {
    write(path, broken);
    assert(ThermalCommand{}.run({"--enable"}) != 0);
    assert(read(path) == broken);
    assert(!GpuManager::selectGpu(4));
    assert(gpuSelect(4) != 0 && read(path) == broken);
    bool failed = false;
    try {
      BenchmarkCommand{}.run({"--backend", "opencl", "--rusticl-enable", "iris"});
    } catch (const std::exception& error) {
      failed = std::string(error.what()).find("OP-BENCH-010") != std::string::npos;
    }
    assert(failed && read(path) == broken);
    assert(std::string(getenv("RUSTICL_ENABLE")) == "original-selector");
  }

  // A blocked destination must report failure, never a successful selection.
  fs::remove(path);
  fs::create_directory(path);
  std::ostringstream output;
  auto* original = std::cout.rdbuf(output.rdbuf());
  assert(gpuSelect(2) != 0);
  std::cout.rdbuf(original);
  assert(output.str().find("Selected GPU") == std::string::npos);
  assert(fs::is_directory(path));
  fs::remove(path);

  // First use still supports creating valid GPU and thermal settings.
  assert(GpuManager::selectGpu(2));
  assert(GpuManager::selectedGpu() == 2);
  fs::remove(path);
  assert(ThermalCommand{}.run({"--enable"}) == 0);
  assert(ConfigurationManager::load().gpu.thermal.enabled);

  if (oldHome) assert(setenv("HOME", savedHome.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  if (oldRusticl) assert(setenv("RUSTICL_ENABLE", savedRusticl.c_str(), 1) == 0);
  else assert(unsetenv("RUSTICL_ENABLE") == 0);
  fs::remove_all(root);
}
