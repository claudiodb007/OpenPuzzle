#pragma once
#include <optional>
#include <string>
namespace openpuzzle {
class ToolManager {
public:
  static std::string configPath();
  static bool configureBitCrack(const std::string &path);
  static bool configureBitCrack(const std::string &cudaPath,
                                const std::string &openclPath);

  static std::optional<std::string> bitcrackPath();
  static std::optional<std::string> bitcrackCudaPath();
  static std::optional<std::string> bitcrackOpenCLPath();
};
} // namespace openpuzzle
