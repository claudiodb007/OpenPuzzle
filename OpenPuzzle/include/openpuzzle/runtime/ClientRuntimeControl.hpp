#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace openpuzzle {

class ClientRuntimeControl {
public:
  static std::filesystem::path pidPath();

  static std::filesystem::path pidPath(
      const std::string& executionSlot);

  static bool acquire();
  static bool release();

  static bool requestStop();

  static bool requestStop(
      const std::string& executionSlot);

  static std::optional<int> runtimePid();

  static std::optional<int> runtimePid(
      const std::string& executionSlot);

  static bool running();

  static bool running(
      const std::string& executionSlot);

private:
  static bool processExists(int pid);
};

} // namespace openpuzzle
