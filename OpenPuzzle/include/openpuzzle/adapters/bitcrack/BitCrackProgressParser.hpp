#pragma once

#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"
#include "openpuzzle/engines/EngineOutputParser.hpp"

namespace openpuzzle::bitcrack {

class BitCrackProgressParser : public openpuzzle::EngineOutputParser {
public:
  std::optional<ExecutionProgress>
  parseLine(const std::string& line) override;

private:
  BitCrackOutputParser parser_;
};

} // namespace openpuzzle::bitcrack
