#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"

using namespace openpuzzle::bitcrack;

int main() {
  BitCrackOutputParser parser;

  auto parsed =
      parser.parse("NVIDIA GeForce R 7918 / 11873MB | 1 target 1319.71 MKey/s "
                   "(104,958,263,296 total) [00:01:18]");

  if (parsed.type != ParsedLineType::Speed)
    return 1;
  if (parsed.speedMKeys != 1319.71)
    return 2;
  if (parsed.totalKeys != "104958263296")
    return 3;

  return 0;
}
