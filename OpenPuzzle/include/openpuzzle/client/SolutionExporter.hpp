#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <string>

namespace openpuzzle::client {

struct SolutionExportResult {
  bool success = false;
  std::string walletPath;
  std::string noticePath;
  std::string format;
  std::string warning;
  std::string error;
};

class SolutionExporter {
public:
  static SolutionExportResult exportSolution(
      const ClientExecutionState &state,
      const std::string &engineResultPath);
};

} // namespace openpuzzle::client
