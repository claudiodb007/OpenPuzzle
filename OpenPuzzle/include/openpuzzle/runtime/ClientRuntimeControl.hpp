#pragma once

#include <filesystem>
#include <optional>

namespace openpuzzle {

class ClientRuntimeControl {
public:
  static std::filesystem::path pidPath();

  static bool acquire();
  static bool release();

  static bool requestStop();

  static std::optional<int> runtimePid();
  static bool running();

private:
  static bool processExists(int pid);
};

} // namespace openpuzzle
