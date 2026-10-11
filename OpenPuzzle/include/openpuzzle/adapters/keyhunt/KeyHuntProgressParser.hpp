#pragma once

#include "openpuzzle/engines/EngineOutputParser.hpp"

namespace openpuzzle::keyhunt {

class KeyHuntProgressParser : public openpuzzle::EngineOutputParser {
public:
  enum class CountProtocol { LegacyCompressedHashes, PrivateScalars };
  explicit KeyHuntProgressParser(CountProtocol protocol =
#ifdef _WIN32
      CountProtocol::PrivateScalars
#else
      CountProtocol::LegacyCompressedHashes
#endif
  ) : protocol_(protocol) {}
  std::optional<ExecutionProgress>
  parseLine(const std::string& line) override;
private:
  CountProtocol protocol_;
};

} // namespace openpuzzle::keyhunt
