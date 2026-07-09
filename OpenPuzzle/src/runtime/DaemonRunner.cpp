#include "openpuzzle/runtime/DaemonRunner.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace openpuzzle {

DaemonRunner::DaemonRunner() = default;

int DaemonRunner::run(int ticks) {
  running_ = true;
  tickCount_ = 0;

  std::cout << "OpenPuzzle Daemon\n";
  std::cout << "-----------------\n";
  std::cout << "Status............ starting\n";

  while (running_ && tickCount_ < ticks) {
    tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::cout << "Status............ stopped\n";

  return 0;
}

void DaemonRunner::stop() {
  running_ = false;
}

void DaemonRunner::tick() {
  ++tickCount_;
  std::cout << "Tick.............. " << tickCount_ << "\n";
}

} // namespace openpuzzle
