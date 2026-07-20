#include "openpuzzle/setup/FirstRunSetup.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <iostream>
#include <string>

namespace openpuzzle {

bool FirstRunSetup::ensureConfigured() const {
  const std::string backend =
      ToolManager::preferredBackend();

  if (backend.empty()) {
    std::cerr
        << "No supported GPU backend was detected.\n";

    return false;
  }

  const auto executable =
      ToolManager::bundledBitCrackPath(
          backend);

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

  config.bitcrack.cudaPath =
      ToolManager::bitcrackCudaPath()
          .value_or("");

  config.bitcrack.openclPath =
      ToolManager::bitcrackOpenCLPath()
          .value_or("");

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
