#pragma once

#include "openpuzzle/engines/EngineOutputParser.hpp"

#include <memory>
#include <string>

namespace openpuzzle {

class EngineParserFactory {
public:
  static std::unique_ptr<EngineOutputParser>
  create(const std::string& engine);

private:
  static std::string normalize(
      std::string value);
};

} // namespace openpuzzle
