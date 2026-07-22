#include "openpuzzle/engines/EngineParserFactory.hpp"

#include "openpuzzle/adapters/bitcrack/BitCrackProgressParser.hpp"
#include "openpuzzle/adapters/keyhunt/KeyHuntProgressParser.hpp"

#include <algorithm>
#include <cctype>

namespace openpuzzle {

std::string EngineParserFactory::normalize(
    std::string value) {
  std::transform(
      value.begin(),
      value.end(),
      value.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  return value;
}

std::unique_ptr<EngineOutputParser>
EngineParserFactory::create(
    const std::string& engine) {
  const auto engineId =
      normalize(engine);

  if (engineId == "bitcrack") {
    return std::make_unique<
        bitcrack::BitCrackProgressParser>();
  }

  if (engineId == "keyhunt") {
    return std::make_unique<
        keyhunt::KeyHuntProgressParser>();
  }

  return nullptr;
}

} // namespace openpuzzle
