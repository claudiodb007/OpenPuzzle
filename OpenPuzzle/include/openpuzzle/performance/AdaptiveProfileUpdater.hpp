#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace openpuzzle {
class Database;
}

namespace openpuzzle::performance {

struct AdaptiveProfileUpdateResult {
  bool attempted{false};
  bool updated{false};
  double sustainedSpeed{0.0};
  double calibratedPlanningSpeed{0.0};
  std::size_t acceptedSamples{0};
  std::string error;
};

class AdaptiveProfileUpdater {
public:
  explicit AdaptiveProfileUpdater(
      Database &database);

  AdaptiveProfileUpdateResult update(
      const client::ClientExecutionState &state,
      const std::vector<double> &speedSamples) const;

private:
  Database &database_;
};

} // namespace openpuzzle::performance
