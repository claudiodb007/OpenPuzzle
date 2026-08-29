#pragma once

#include "openpuzzle/engines/common/PuzzleExecutionPlanner.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class PuzzleMetadataCatalog {
public:
  static std::optional<PuzzleExecutionMetadata> load(int puzzle);
  static std::optional<PuzzleExecutionMetadata> parse(
      int expectedPuzzle,
      const std::string &json);

private:
  static std::optional<std::string> readPuzzleFile(int puzzle);
};

} // namespace openpuzzle
