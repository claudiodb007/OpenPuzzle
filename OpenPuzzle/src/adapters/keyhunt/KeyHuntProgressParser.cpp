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
      if(protocol_==CountProtocol::PrivateScalars &&
          candidateTotal>cpp_int("18446744073709551615"))return std::nullopt;
      if(protocol_==CountProtocol::LegacyCompressedHashes)candidateTotal /= 2;
      progress.keysChecked =
          candidateTotal.str();

      const double candidateKeysPerSecond =
          std::stod(match[2].str());

      progress.speedMKeys = candidateKeysPerSecond / 1000000.0;
      if(protocol_==CountProtocol::LegacyCompressedHashes)progress.speedMKeys /= 2.0;
    } catch (...) {
      return std::nullopt;
    }

    return progress;
  }

  if(protocol_==CountProtocol::PrivateScalars) {
  // Protocol 1 proves the exact scalar count. A bare End is not this proof.
  static const std::regex completed(R"(^OPENPUZZLE_RANGE_DONE keys_checked=([0-9]+)$)");
  if(std::regex_match(line,match,completed)) {
    try {
      const boost::multiprecision::cpp_int count(match[1].str());
      if(count<0||count>boost::multiprecision::cpp_int("18446744073709551615"))return std::nullopt;
      ExecutionProgress progress;progress.finished=true;progress.keysChecked=count.str();
      progress.message="OPENPUZZLE_RANGE_DONE";return progress;
    }catch(...){return std::nullopt;}
  }
  } else if (std::regex_match(
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
