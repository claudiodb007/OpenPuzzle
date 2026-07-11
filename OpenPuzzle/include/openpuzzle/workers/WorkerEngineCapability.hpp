#pragma once

#include <string>

namespace openpuzzle {

struct WorkerEngineCapability {
  std::string engine;
  std::string backend;

  int device = 0;
  int vramMb = 0;

  int blocks = 0;
  int threads = 0;
  int points = 0;

  double benchmarkSpeedMkeys = 0.0;

  bool available = true;
  bool supportsCompressed = true;
  bool supportsUncompressed = true;

  bool matches(const std::string& requiredEngine,
               const std::string& requiredBackend) const {
    if (!available) {
      return false;
    }

    if (!requiredEngine.empty() && engine != requiredEngine) {
      return false;
    }

    if (!requiredBackend.empty() && backend != requiredBackend) {
      return false;
    }

    return true;
  }

  bool hasLaunchProfile() const {
    if (backend == "CPU" ||
        backend == "cpu") {
      return threads > 0;
    }

    return blocks > 0 &&
           threads > 0 &&
           points > 0;
  }
};

} // namespace openpuzzle
