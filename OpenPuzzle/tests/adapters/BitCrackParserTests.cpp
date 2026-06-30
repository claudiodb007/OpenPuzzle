#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"

using namespace openpuzzle::bitcrack;

int main() {
  BitCrackOutputParser parser;

  auto speed = parser.parse("[Info] 1334.62 MKey/s");
  if (speed.type != ParsedLineType::Speed)
    return 1;
  if (speed.speedMKeys != 1334.62)
    return 2;

  auto start = parser.parse("[Info] Starting at: 400000000000000000");
  if (start.type != ParsedLineType::StartingKey)
    return 3;
  if (start.value != "400000000000000000")
    return 4;

  auto end = parser.parse("[Info] Ending at: 7FFFFFFFFFFFFFFFFF");
  if (end.type != ParsedLineType::EndingKey)
    return 5;
  if (end.value != "7FFFFFFFFFFFFFFFFF")
    return 6;

  auto finished = parser.parse("Finished");
  if (finished.type != ParsedLineType::Finished)
    return 7;

  auto error = parser.parse("[Error] Something failed");
  if (error.type != ParsedLineType::Error)
    return 8;

  return 0;
}
