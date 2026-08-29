#include "openpuzzle/engines/common/PuzzleExecutionPlanner.hpp"

#include "openpuzzle/engines/common/SearchMode.hpp"

namespace openpuzzle {

PuzzleExecutionPlan PuzzleExecutionPlanner::plan(
    const PuzzleExecutionMetadata &metadata,
    bool cudaAvailable,
    bool openclAvailable,
    bool cpuAvailable,
    bool kangarooExecutorAvailable) {
  (void)cpuAvailable;

  if (searchModeFromString(metadata.searchMode) == SearchMode::Kangaroo) {
    if (metadata.publicKey.empty())
      return {"kangaroo", "cuda", false,
              "kangaroo search requires puzzle public_key metadata"};

    if (!metadata.requiredBackend.empty() &&
        metadata.requiredBackend != "cuda")
      return {"kangaroo", "cuda", false,
              "kangaroo search requires CUDA"};

    if (!cudaAvailable)
      return {"kangaroo", "cuda", false,
              "CUDA device is required for kangaroo search"};

    if (!kangarooExecutorAvailable)
      return {"kangaroo", "cuda", false,
              "PSCKangaroo executable is not installed; run: "
              "openpuzzle engine install psckangaroo"};

    return {"kangaroo", "cuda", true, {}};
  }

  if (cudaAvailable)
    return {"bitcrack", "cuda", true, {}};

  if (openclAvailable)
    return {"bitcrack", "opencl", true, {}};

  return {"bitcrack", "", false,
          "no compatible GPU backend is available"};
}

} // namespace openpuzzle
