#pragma once

#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <string>

namespace openpuzzle {

struct ExecutionResource {
  std::string id;
  std::string name;

  std::string engine;
  std::string backend;

  int device = 0;
  int memoryMb = 0;

  WorkerEngineCapability capability;

  bool available = true;

  bool matches(const std::string& requiredEngine,
               const std::string& requiredBackend) const {
    if (!available) {
      return false;
    }

    return capability.matches(
        requiredEngine,
        requiredBackend);
  }
};

} // namespace openpuzzle
