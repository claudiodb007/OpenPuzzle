#pragma once

#include "openpuzzle/engines/EngineOutputParser.hpp"

namespace openpuzzle::kangaroo {

class KangarooProgressParser final : public EngineOutputParser {
public:
  std::optional<ExecutionProgress>
  parseLine(const std::string& line) override;
};

} // namespace openpuzzle::kangaroo
