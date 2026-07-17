#pragma once

#include <filesystem>

namespace openpuzzle {

class WorkspaceSecurity {
public:
  static void prepare(
      const std::filesystem::path &workspace);

  static void protectFile(
      const std::filesystem::path &file);
};

} // namespace openpuzzle
