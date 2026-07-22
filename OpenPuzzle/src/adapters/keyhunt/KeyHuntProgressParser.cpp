#include "openpuzzle/adapters/keyhunt/KeyHuntProgressParser.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <regex>
#include <string>

namespace openpuzzle::keyhunt {

std::optional<ExecutionProgress>
KeyHuntProgressParser::parseLine(
    const std::string& line) {
  static const std::regex progressPattern(
      R"(Total\s+([0-9]+)\s+keys.*\(([0-9]+)\s+keys/s\))");

  static const std::regex finishedPattern(
      R"(^\s*End\s*$)");

  std::smatch match;

  if (std::regex_search(
          line,
          match,
          progressPattern)) {
    using boost::multiprecision::cpp_int;

    ExecutionProgress progress;

    try {
      cpp_int candidateTotal(
          match[1].str());

      /*
       * In compressed-address mode KeyHunt tests both
       * possible public-key prefixes for each private
       * scalar and counts both hashes. OpenPuzzle reports
       * private scalars, so the public counter is divided
       * by two.
       */
      candidateTotal /= 2;
      progress.keysChecked =
          candidateTotal.str();

      const double candidateKeysPerSecond =
          std::stod(match[2].str());

      progress.speedMKeys =
          candidateKeysPerSecond /
          2.0 /
          1000000.0;
    } catch (...) {
      return std::nullopt;
    }

    return progress;
  }

  if (std::regex_match(
          line,
          finishedPattern)) {
    ExecutionProgress progress;
    progress.finished = true;
    progress.message = "End";
    return progress;
  }

  if (line.find("[E]") !=
      std::string::npos) {
    ExecutionProgress progress;
    progress.error = true;
    progress.message = line;
    return progress;
  }

  return std::nullopt;
}

} // namespace openpuzzle::keyhunt
