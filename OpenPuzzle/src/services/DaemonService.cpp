#include "openpuzzle/services/DaemonService.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace openpuzzle {

int DaemonService::execute(const std::vector<std::string>& args) {
  int ticks = 3;

  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == "--ticks") {
      ticks = std::stoi(args[i + 1]);
    }
  }

  std::cout << "OpenPuzzle Daemon\n";
  std::cout << "-----------------\n";
  std::cout << "Status............ starting\n";

  for (int tick = 1; tick <= ticks; ++tick) {
    std::cout << "Tick.............. " << tick << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::cout << "Status............ stopped\n";

  return 0;
}

} // namespace openpuzzle
