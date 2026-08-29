#include "openpuzzle/adapters/kangaroo/KangarooProgressParser.hpp"
#include "openpuzzle/engines/EngineParserFactory.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace openpuzzle;

namespace {

bool closeTo(double actual, double expected) {
  return std::fabs(actual - expected) < 0.000001;
}

} // namespace

int main() {
  kangaroo::KangarooProgressParser parser;

  auto trap = parser.parseLine(
      "TRAP: Speed: 4.37 GKeys/s | DPs: 123M | Time: 0d 01h 02m\r");
  assert(trap);
  assert(closeTo(trap->speedMKeys, 4370.0));
  assert(trap->keysChecked.empty());
  assert(!trap->finished);
  assert(!trap->keyFound);

  auto hunt = parser.parseLine(
      "HUNT: Speed: 2.50 GKeys/s | DPs: 456M | Time: 0d 03h 04m");
  assert(hunt);
  assert(closeTo(hunt->speedMKeys, 2500.0));
  assert(hunt->keysChecked.empty());

  auto concurrent = parser.parseLine(
      "CONC: Speed: 0.75 GKeys/s | Time: 0d 00h 10m");
  assert(concurrent);
  assert(closeTo(concurrent->speedMKeys, 750.0));

  auto allWild = parser.parseLine(
      "  Speed: 1.25 GKeys/s | Time: 0d 00h 20m | Wave #7");
  assert(allWild);
  assert(closeTo(allWild->speedMKeys, 1250.0));

  assert(!parser.parseLine(
      "  HUNT Wave #7: Checks: 123M, T-W: 2, W-W: 1, FP: 0"));
  assert(!parser.parseLine("Exit completed."));
  assert(!parser.parseLine(
      "Exiting cleanly (checkpoint disabled by -checkpoint 0)."));
  assert(!parser.parseLine("PRIVATE KEY: synthetic-secret"));
  assert(!parser.parseLine("Key saved to RESULTS.TXT"));

  auto error = parser.parseLine("ERROR: synthetic failure\r");
  assert(error);
  assert(error->error);
  assert(error->message == "ERROR: synthetic failure");

  auto factory = EngineParserFactory::create("Kangaroo");
  assert(factory);
  auto factoryProgress = factory->parseLine(
      "HUNT: Speed: 3.00 GKeys/s | DPs: 1M | Time: 0d 00h 01m");
  assert(factoryProgress);
  assert(closeTo(factoryProgress->speedMKeys, 3000.0));

  assert(EngineParserFactory::create("kangaroo"));
  assert(EngineParserFactory::create("PSCKangaroo"));

  std::cout << "KangarooProgressParserTests passed\n";
  return 0;
}
