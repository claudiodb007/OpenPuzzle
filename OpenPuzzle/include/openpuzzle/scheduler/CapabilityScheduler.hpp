#pragma once

#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class WorkerAgent;
class WorkerAgentRegistry;

struct CapabilitySelection {
  int workerId = 0;

  std::string engine;
  std::string backend;

  WorkerEngineCapability capability;

  double expectedSpeedMkeys = 0.0;

  bool valid = false;
};

class CapabilityScheduler {
public:
  explicit CapabilityScheduler(
      WorkerAgentRegistry& workers);

  std::optional<CapabilitySelection> select(
      const std::string& engine,
      const std::string& backend) const;

private:
  WorkerAgentRegistry& workers_;
};

} // namespace openpuzzle
