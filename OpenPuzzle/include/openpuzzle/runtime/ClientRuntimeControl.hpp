#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

class ClientRuntimeControl {
public:
  static std::filesystem::path pidPath();

  static std::filesystem::path pidPath(
      const std::string& executionSlot);

  static std::filesystem::path safeStopPath();

  static std::filesystem::path safeStopPath(
      const std::string& executionSlot);

  static bool acquire();
  static bool release();

  static bool requestStop();

  static bool requestStop(
      const std::string& executionSlot);

  static bool requestSafeStop();

  static bool requestSafeStop(
      const std::string& executionSlot);

  static bool safeStopRequested();

  static bool safeStopRequested(
      const std::string& executionSlot);

  static bool clearSafeStop();

  static bool clearSafeStop(
      const std::string& executionSlot);

  static std::optional<int> runtimePid();

  static std::optional<int> runtimePid(
      const std::string& executionSlot);

  static std::optional<std::string> runtimeBootId();

  static std::optional<std::string> runtimeBootId(
      const std::string& executionSlot);

  static std::optional<std::uint64_t>
  runtimeStartTime();

  static std::optional<std::uint64_t>
  runtimeStartTime(
      const std::string& executionSlot);

  static bool running();

  static bool running(
      const std::string& executionSlot);

  // Unavailable is not evidence that the slot is free or the runtime stopped.
  enum class RuntimeIdentityStatus {
    Inactive,
    Running,
    Unavailable
  };

  static std::vector<std::string> executionSlots();

  static RuntimeIdentityStatus runtimeIdentityStatus(
      const std::string& executionSlot);

#ifdef _WIN32
  // Cooperative Windows stop requests are bound to PID + creation FILETIME.
  // The runtime loop must poll these; a request never forcibly terminates it.
  static std::filesystem::path stopPath(const std::string& executionSlot);
  static bool stopRequested(const std::string& executionSlot);
  static bool clearStop(const std::string& executionSlot);
#endif
};

} // namespace openpuzzle
