#include "openpuzzle/runtime/KangarooWalkSeed.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_set>

using namespace openpuzzle;

int main() {
  assert(KangarooWalkSeed::valid("0123456789ABCDEF"));
  assert(KangarooWalkSeed::valid("abcdef0123456789"));
  assert(!KangarooWalkSeed::valid(""));
  assert(!KangarooWalkSeed::valid("0000000000000000"));
  assert(!KangarooWalkSeed::valid("0123456789ABCDE"));
  assert(!KangarooWalkSeed::valid("0123456789ABCDEG"));

  std::unordered_set<std::string> generated;
  for (int index = 0; index < 256; ++index) {
    const auto seed = KangarooWalkSeed::generate();
    assert(KangarooWalkSeed::valid(seed));
    assert(generated.insert(seed).second);
  }

  std::cout << "KangarooWalkSeedTests passed\n";
  return 0;
}
