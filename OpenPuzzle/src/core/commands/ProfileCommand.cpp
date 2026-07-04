#include "openpuzzle/core/commands/ProfileCommand.hpp"

#include "openpuzzle/database/Database.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace openpuzzle {

static std::string dbPath() {
  const char *home = std::getenv("HOME");

  if (!home)
    throw std::runtime_error("HOME not set");

  return std::string(home) + "/.local/share/OpenPuzzle/openpuzzle.db";
}

int ProfileCommand::run(const std::vector<std::string> &args) const {
  std::string subcommand = args.empty() ? "list" : args[0];

  if (subcommand != "list") {
    std::cerr << "Usage: OpenPuzzle profile list\n";
    return 1;
  }

  Database db;
  if (!db.open(dbPath()) || !db.createSchema()) {
    std::cerr << "Database error\n";
    return 1;
  }

  auto profiles = db.listGpuProfiles();

  std::cout << "====================================\n";
  std::cout << "      OpenPuzzle GPU Profiles\n";
  std::cout << "====================================\n\n";

  if (profiles.empty()) {
    std::cout << "No GPU profiles found.\n";
    return 0;
  }

  for (const auto &profile : profiles) {
    std::cout << "ID................ " << profile.id << "\n";
    std::cout << "GPU............... " << profile.gpuName << "\n";
    std::cout << "Backend........... " << profile.backend << "\n";
    std::cout << "Engine............ " << profile.engine << "\n";
    std::cout << "Blocks............ " << profile.blocks << "\n";
    std::cout << "Threads........... " << profile.threads << "\n";
    std::cout << "Points............ " << profile.points << "\n";
    std::cout << "Average........... " << profile.averageSpeed << " MKey/s\n";
    std::cout << "Minimum........... " << profile.minimumSpeed << " MKey/s\n";
    std::cout << "Maximum........... " << profile.maximumSpeed << " MKey/s\n";
    std::cout << "Samples........... " << profile.samples << "\n\n";
  }

  return 0;
}

} // namespace openpuzzle
