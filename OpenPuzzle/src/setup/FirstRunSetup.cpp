#include "openpuzzle/setup/FirstRunSetup.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

bool executableFile(const std::string &path) {
  return !path.empty() && fs::is_regular_file(path) &&
         access(path.c_str(), X_OK) == 0;
}

std::string findInPath(const std::string &executable) {
  const char *pathValue = std::getenv("PATH");

  if (!pathValue) {
    return {};
  }

  std::stringstream stream(pathValue);
  std::string directory;

  while (std::getline(stream, directory, ':')) {
    if (directory.empty()) {
      continue;
    }

    const auto candidate = fs::path(directory) / executable;

    if (executableFile(candidate.string())) {
      return candidate.string();
    }
  }

  return {};
}

std::string findExecutable(const std::string &name,
                           const std::string &configured) {
  if (executableFile(configured)) {
    return configured;
  }

  const auto fromPath = findInPath(name);

  if (!fromPath.empty()) {
    return fromPath;
  }

  const char *home = std::getenv("HOME");

  if (!home) {
    return {};
  }

  const std::vector<fs::path> candidates = {
      fs::path(home) / "BitCrack/bin" / name,

      fs::path(home) / "BitCrack" / name,

      fs::path(home) / "bin" / name,
  };

  for (const auto &candidate : candidates) {
    if (executableFile(candidate.string())) {
      return candidate.string();
    }
  }

  return {};
}

std::string askExecutable(const std::string &label,
                          const std::string &detected) {
  while (true) {
    std::cout << label;

    if (!detected.empty()) {
      std::cout << "\n[" << detected << "]";
    }

    std::cout << "\n> ";

    std::string value;
    std::getline(std::cin, value);

    if (value.empty()) {
      value = detected;
    }

    if (executableFile(value)) {
      return value;
    }

    std::cerr << "Executable not found or "
              << "not executable.\n\n";
  }
}

} // namespace

bool FirstRunSetup::ensureConfigured() const {
  auto config = ConfigurationManager::load();

  if (!config.engine.id.empty() && !config.engine.backend.empty() &&
      executableFile(config.engine.executable)) {
    return true;
  }

  const std::string cudaDetected =
      findExecutable("cuBitCrack", config.bitcrack.cudaPath);

  const std::string openclDetected =
      findExecutable("clBitCrack", config.bitcrack.openclPath);

  std::cout << "OpenPuzzle First Setup\n"
            << "----------------------\n\n"
            << "Choose search engine:\n\n"
            << "1. BitCrack CUDA";

  if (!cudaDetected.empty()) {
    std::cout << "  [detected]";
  }

  std::cout << "\n2. BitCrack OpenCL";

  if (!openclDetected.empty()) {
    std::cout << "  [detected]";
  }

  std::cout << "\n\nSelection [1]: ";

  std::string selection;
  std::getline(std::cin, selection);

  if (selection.empty()) {
    selection = "1";
  }

  if (selection != "1" && selection != "2") {
    std::cerr << "Invalid engine selection.\n";

    return false;
  }

  config.engine.id = "bitcrack";

  if (selection == "1") {
    config.engine.backend = "cuda";

    config.engine.executable =
        askExecutable("BitCrack CUDA executable", cudaDetected);

    config.bitcrack.cudaPath = config.engine.executable;

    if (config.bitcrack.openclPath.empty()) {
      config.bitcrack.openclPath =
          (fs::path(config.engine.executable).parent_path() / "clBitCrack")
              .string();
    }
  } else {
    config.engine.backend = "opencl";

    config.engine.executable =
        askExecutable("BitCrack OpenCL executable", openclDetected);

    config.bitcrack.openclPath = config.engine.executable;

    if (config.bitcrack.cudaPath.empty()) {
      config.bitcrack.cudaPath =
          (fs::path(config.engine.executable).parent_path() / "cuBitCrack")
              .string();
    }
  }

  config.assignment.durationMinutes = 60;

  if (!ConfigurationManager::save(config)) {
    std::cerr << "Unable to save configuration.\n";

    return false;
  }

  std::cout << "\nConfiguration saved.\n"
            << "Engine............. BitCrack\n"
            << "Backend............ "
            << (config.engine.backend == "cuda" ? "CUDA" : "OpenCL") << "\n\n";

  return true;
}

} // namespace openpuzzle
