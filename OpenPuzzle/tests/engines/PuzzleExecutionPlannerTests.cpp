#include "openpuzzle/engines/common/PuzzleExecutionPlanner.hpp"

#include <cassert>

using openpuzzle::PuzzleExecutionMetadata;
using openpuzzle::PuzzleExecutionPlanner;

int main() {
  {
    PuzzleExecutionMetadata puzzle;
    puzzle.puzzle = 71;
    puzzle.searchMode = "linear";

    const auto plan =
        PuzzleExecutionPlanner::plan(puzzle, true, true, true);

    assert(plan.engine == "bitcrack");
    assert(plan.backend == "cuda");
    assert(plan.executable);
  }

  {
    PuzzleExecutionMetadata puzzle;
    puzzle.puzzle = 140;
    puzzle.searchMode = "kangaroo";
    puzzle.publicKey = "synthetic-public-key";
    puzzle.requiredBackend = "cuda";

    const auto plan =
        PuzzleExecutionPlanner::plan(puzzle, true, true, true);

    assert(plan.engine == "kangaroo");
    assert(plan.backend == "cuda");
    assert(!plan.executable);
    assert(plan.reason ==
           "PSCKangaroo executable is not installed; run: "
           "openpuzzle engine install psckangaroo");
  }


  {
    PuzzleExecutionMetadata puzzle;
    puzzle.puzzle = 140;
    puzzle.searchMode = "kangaroo";
    puzzle.publicKey =
        "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640";
    puzzle.requiredBackend = "cuda";

    const auto plan =
        PuzzleExecutionPlanner::plan(
            puzzle,
            true,
            true,
            true,
            true);

    assert(plan.engine == "kangaroo");
    assert(plan.backend == "cuda");
    assert(plan.executable);
    assert(plan.reason.empty());
  }

  {
    PuzzleExecutionMetadata puzzle;
    puzzle.puzzle = 140;
    puzzle.searchMode = "kangaroo";
    puzzle.requiredBackend = "cuda";

    const auto plan =
        PuzzleExecutionPlanner::plan(puzzle, true, true, true);

    assert(!plan.executable);
    assert(plan.reason ==
           "kangaroo search requires puzzle public_key metadata");
  }

  {
    PuzzleExecutionMetadata puzzle;
    puzzle.puzzle = 145;
    puzzle.searchMode = "kangaroo";
    puzzle.publicKey = "synthetic-public-key";
    puzzle.requiredBackend = "cuda";

    const auto plan =
        PuzzleExecutionPlanner::plan(puzzle, false, true, true);

    assert(plan.engine == "kangaroo");
    assert(plan.backend == "cuda");
    assert(!plan.executable);
    assert(plan.reason ==
           "CUDA device is required for kangaroo search");
  }

  return 0;
}
