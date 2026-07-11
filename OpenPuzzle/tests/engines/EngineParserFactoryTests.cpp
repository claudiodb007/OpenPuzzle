#include "openpuzzle/engines/EngineParserFactory.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle;

int main() {
  auto bitcrack =
      EngineParserFactory::create(
          "BitCrack");

  assert(bitcrack);

  auto lowercase =
      EngineParserFactory::create(
          "bitcrack");

  assert(lowercase);

  auto uppercase =
      EngineParserFactory::create(
          "BITCRACK");

  assert(uppercase);

  auto unknown =
      EngineParserFactory::create(
          "unknown-engine");

  assert(!unknown);

  auto progress =
      bitcrack->parseLine(
          "[Info] 1334.62 MKey/s "
          "(1,000,000 total)");

  assert(progress);
  assert(progress->speedMKeys == 1334.62);
  assert(progress->keysChecked == "1000000");

  std::cout
      << "EngineParserFactoryTests passed\n";

  return 0;
}
