#include "openpuzzle/adapters/kangaroo/KangarooProgressParser.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace openpuzzle::kangaroo {
namespace {

std::string lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });
  return value;
}

std::string trim(std::string value) {
  const auto notSpace = [](unsigned char character) {
    return !std::isspace(character);
  };

  value.erase(
      value.begin(),
      std::find_if(value.begin(), value.end(), notSpace));
  value.erase(
      std::find_if(value.rbegin(), value.rend(), notSpace).base(),
      value.end());
  return value;
}

} // namespace

std::optional<ExecutionProgress>
KangarooProgressParser::parseLine(const std::string& line) {
  const auto normalized = lower(line);

  // Never expose private-key material through live progress.
  if (normalized.find("private key") != std::string::npos ||
      normalized.find("key saved to results.txt") != std::string::npos) {
    return std::nullopt;
  }

  static const std::regex speedPattern(
      R"(Speed:\s*([0-9]+(?:\.[0-9]+)?)\s*GKeys/s)",
      std::regex::icase);

  std::smatch speedMatch;
  if (std::regex_search(line, speedMatch, speedPattern)) {
    ExecutionProgress progress;
    progress.speedMKeys = std::stod(speedMatch[1].str()) * 1000.0;

    // PSCK's DPs and Checks counters are not linear keys checked.
    // Zero is the explicit wire-level sentinel for unknown/not-applicable
    // linear coverage. It keeps the assignment lease alive without claiming
    // deterministic keyspace coverage.
    progress.keysChecked = "0";
    return progress;
  }

  if (line.find("ERROR:") != std::string::npos ||
      line.find("[Error]") != std::string::npos) {
    ExecutionProgress progress;
    progress.error = true;
    progress.message = trim(line);
    return progress;
  }

  // "Exit completed." is also emitted after a clean user stop and must
  // never be interpreted as assignment completion.
  return std::nullopt;
}

} // namespace openpuzzle::kangaroo
