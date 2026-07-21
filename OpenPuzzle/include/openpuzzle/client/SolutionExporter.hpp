#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <string>

namespace openpuzzle::client {

struct SolutionExportResult {
  bool success = false;
  std::string walletPath;
  std::string error;
};

class SolutionExporter {
public:
  static SolutionExportResult exportSolution(
      const ClientExecutionState &state,
      const std::string &engineResultPath);
};

} // namespace openpuzzle::client
