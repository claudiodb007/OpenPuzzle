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

  auto keyhunt =
      EngineParserFactory::create(
          "KeyHunt");

  assert(keyhunt);

  auto cpuProgress =
      keyhunt->parseLine(
          "[+] Total 33554432 keys in 2 seconds: "
          "~16 Mkeys/s (16777216 keys/s)");

  assert(cpuProgress);
  assert(
      cpuProgress->keysChecked ==
      "16777216");
  assert(
      cpuProgress->speedMKeys > 8.388607 &&
      cpuProgress->speedMKeys < 8.388609);

  auto cpuFinished =
      keyhunt->parseLine(
          "End");

  assert(cpuFinished);
  assert(cpuFinished->finished);

  auto sensitive =
      keyhunt->parseLine(
          "Hit! Private Key: synthetic-secret");

  assert(!sensitive);

  std::cout
      << "EngineParserFactoryTests passed\n";

  return 0;
}
