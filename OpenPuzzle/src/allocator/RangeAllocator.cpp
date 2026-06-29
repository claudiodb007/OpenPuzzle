#include "openpuzzle/allocator/RangeAllocator.hpp"

namespace openpuzzle {

RangeAllocator::RangeAllocator(Database& db) : db_(db) {}

bool RangeAllocator::overlaps(const KeySpace& a, const KeySpace& b) {
    return a.start() <= b.end() && b.start() <= a.end();
}

std::optional<RangeRecord> RangeAllocator::allocateNext(const PuzzleRecord& puzzle, int blockBits) {
    KeySpace puzzleSpace = KeySpace::fromHexPair(puzzle.rangeStart, puzzle.rangeEnd);

    if (!puzzleSpace.valid() || blockBits <= 0) {
        return std::nullopt;
    }

    KeySpace::Int blockSize = KeySpace::Int(1);
    blockSize <<= blockBits;

    const auto localRanges = db_.listRanges(puzzle.id);
    const auto externalRanges = db_.listExternalRanges(puzzle.id);

    KeySpace::Int candidateStart = puzzleSpace.start();

    while (candidateStart <= puzzleSpace.end()) {
        KeySpace::Int candidateEnd = candidateStart + blockSize;
        candidateEnd -= 1;

        if (candidateEnd > puzzleSpace.end()) {
            candidateEnd = puzzleSpace.end();
        }

        KeySpace candidate(candidateStart, candidateEnd);
        bool blocked = false;

        for (const auto& range : localRanges) {
            KeySpace existing = KeySpace::fromHexPair(range.startKey, range.endKey);

            if (overlaps(candidate, existing)) {
                candidateStart = existing.end();
                candidateStart += 1;
                blocked = true;
                break;
            }
        }

        if (blocked) {
            continue;
        }

        for (const auto& range : externalRanges) {
            KeySpace existing = KeySpace::fromHexPair(range.startKey, range.endKey);

            if (overlaps(candidate, existing)) {
                candidateStart = existing.end();
                candidateStart += 1;
                blocked = true;
                break;
            }
        }

        if (blocked) {
            continue;
        }

        RangeRecord reservedRange;
        reservedRange.puzzleId = puzzle.id;
        reservedRange.startKey = candidate.startHex();
        reservedRange.endKey = candidate.endHex();
        reservedRange.blockBits = blockBits;
        reservedRange.status = RangeStatus::Reserved;
        reservedRange.id = db_.insertRange(reservedRange);

        if (reservedRange.id <= 0) {
            return std::nullopt;
        }

        return reservedRange;
    }

    return std::nullopt;
}

} // namespace openpuzzle
