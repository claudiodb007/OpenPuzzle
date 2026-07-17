#pragma once

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"

#include <string>

namespace openpuzzle {

class ExecutionPersistence {
public:
  void writeExecutionFile(const ExecutionContext& context) const;

  void writeStateFile(const ExecutionContext& context,
                      const std::string& status,
                      const ExecutionResult& result) const;
};

} // namespace openpuzzle
