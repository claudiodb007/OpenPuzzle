#include "openpuzzle/adapters/bitcrack/BitCrackProgressParser.hpp"

namespace openpuzzle::bitcrack {

std::optional<ExecutionProgress>
BitCrackProgressParser::parseLine(const std::string& line) {
  auto parsed = parser_.parse(line);

  ExecutionProgress progress;

  switch (parsed.type) {
  case ParsedLineType::Speed:
    progress.speedMKeys = parsed.speedMKeys;
    progress.keysChecked = parsed.totalKeys;
    return progress;

  case ParsedLineType::Found:
    progress.keyFound = true;
    progress.privateKey = parsed.value;
    progress.message = parsed.value;
    return progress;

  case ParsedLineType::Error:
    progress.error = true;
    progress.message = parsed.value;
    return progress;

  case ParsedLineType::Finished:
    progress.finished = true;
    progress.message = parsed.value;
    return progress;

  default:
    return std::nullopt;
  }
}

} // namespace openpuzzle::bitcrack
