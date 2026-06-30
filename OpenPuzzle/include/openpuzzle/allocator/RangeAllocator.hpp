#pragma once
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/models/KeySpace.hpp"
namespace openpuzzle {
class RangeAllocator {
public:
  explicit RangeAllocator(Database &db);
  std::optional<RangeRecord> allocateNext(const PuzzleRecord &puzzle,
                                          int blockBits);

private:
  Database &db_;
  static bool overlaps(const KeySpace &a, const KeySpace &b);
};
} // namespace openpuzzle
