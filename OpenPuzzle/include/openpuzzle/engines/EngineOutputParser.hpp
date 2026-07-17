#pragma once

#include "openpuzzle/runtime/ExecutionProgress.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class EngineOutputParser {
public:
  virtual ~EngineOutputParser() = default;

  virtual std::optional<ExecutionProgress>
  parseLine(const std::string& line) = 0;
};

} // namespace openpuzzle
