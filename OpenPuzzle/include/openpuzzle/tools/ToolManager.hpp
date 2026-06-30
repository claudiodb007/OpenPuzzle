#pragma once
#include <optional>
#include <string>
namespace openpuzzle {
class ToolManager {
public:
  static std::string configPath();
  static bool configureBitCrack(const std::string &path);
  static std::optional<std::string> bitcrackPath();
};
} // namespace openpuzzle
