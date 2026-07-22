#pragma once

#include "openpuzzle/engines/EngineOutputParser.hpp"

namespace openpuzzle::keyhunt {

class KeyHuntProgressParser : public openpuzzle::EngineOutputParser {
public:
  std::optional<ExecutionProgress>
  parseLine(const std::string& line) override;
};

} // namespace openpuzzle::keyhunt
