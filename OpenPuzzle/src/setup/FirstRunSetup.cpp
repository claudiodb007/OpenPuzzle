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
        << "OpenPuzzle setup failed\n"
        << "-----------------------\n"
        << "Error code......... OP-SETUP-001\n"
        << "Problem............ no supported GPU backend was detected\n"
        << "Action 1........... verify the NVIDIA/OpenCL driver\n"
        << "Action 2........... run: openpuzzle doctor\n";

    return false;
  }

  const auto executable =
      ToolManager::bundledBitCrackPath(
          backend);

  if (!executable) {
    std::cerr
        << "OpenPuzzle setup failed\n"
        << "-----------------------\n"
        << "Error code......... OP-ENGINE-001\n"
        << "Problem............ bundled OpenPuzzle-BitCrack "
        << (backend == "opencl" ? "OpenCL" : "CUDA")
        << " engine is missing or invalid\n"
        << "Action 1........... reinstall the OpenPuzzle package\n"
        << "Action 2........... run: openpuzzle doctor\n";

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
    std::cerr
        << "OpenPuzzle setup failed\n"
        << "-----------------------\n"
        << "Error code......... OP-CONFIG-001\n"
        << "Problem............ unable to save the local configuration\n"
        << "Configuration...... "
        << ToolManager::configPath()
        << '\n'
        << "Action............. check ownership and write permissions\n";

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
