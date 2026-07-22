#pragma once

#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

class ToolManager {
public:
  static std::string configPath();

  static bool configureBitCrack(
      const std::string &path);

  static bool configureBitCrack(
      const std::string &cudaPath,
      const std::string &openclPath);

  static bool supportsBackend(
      const std::string &backend);

  static std::vector<std::string>
  bundledBackends();

  /*
   * CUDA is preferred when its engine reports at
   * least one usable device. OpenCL is the fallback.
   */
  static std::string preferredBackend();

  /* Compatibility name used by existing callers. */
  static std::string bundledBackend();

  static std::optional<std::string>
  bundledBitCrackPath();

  static std::optional<std::string>
  bundledBitCrackPath(
      const std::string &backend);

  static bool validateBitCrackEngine(
      const std::string &path,
      const std::string &backend,
      std::string *error = nullptr);

  static std::optional<std::string> bitcrackPath();
  static std::optional<std::string> bitcrackCudaPath();
  static std::optional<std::string> bitcrackOpenCLPath();
  static std::optional<std::string> keyhuntPath();
};

} // namespace openpuzzle
