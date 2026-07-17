#pragma once

#include <filesystem>

namespace openpuzzle {

class WorkspaceSecurity {
public:
  static void prepare(
      const std::filesystem::path &workspace);
};

} // namespace openpuzzle
