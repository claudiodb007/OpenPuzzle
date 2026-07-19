#include "openpuzzle/setup/FirstRunSetup.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <iostream>
#include <string>

namespace openpuzzle {

bool FirstRunSetup::ensureConfigured() const {
  const std::string backend =
      ToolManager::bundledBackend();

  const auto executable =
      ToolManager::bundledBitCrackPath();

  if (!executable) {
    std::cerr
        << "The bundled OpenPuzzle-BitCrack "
        << (backend == "opencl" ? "OpenCL" : "CUDA")
        << " engine is missing or invalid.\n";

    return false;
  }

  auto config =
      ConfigurationManager::load();

  const bool changed =
      config.engine.id != "bitcrack" ||
      config.engine.backend != backend ||
      config.engine.executable != *executable;

  config.engine.id = "bitcrack";
  config.engine.backend = backend;
  config.engine.executable = *executable;

  config.bitcrack.cudaPath.clear();
  config.bitcrack.openclPath.clear();

  if (backend == "opencl") {
    config.bitcrack.openclPath = *executable;
  } else {
    config.bitcrack.cudaPath = *executable;
  }

  if (!ConfigurationManager::save(config)) {
    std::cerr << "Unable to save configuration.\n";

    return false;
  }

  if (changed) {
    std::cout
        << "OpenPuzzle engine configured\n"
        << "----------------------------\n"
        << "Engine............. OpenPuzzle-BitCrack\n"
        << "Backend............ "
        << (backend == "opencl" ? "OpenCL" : "CUDA")
        << '\n'
        << "Executable......... "
        << *executable
        << "\n\n";
  }

  return true;
}

} // namespace openpuzzle
