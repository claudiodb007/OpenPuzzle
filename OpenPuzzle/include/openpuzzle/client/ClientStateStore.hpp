#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace openpuzzle::client {

class ClientStateStore {
public:
  static std::string executionSlot();

  /*
   * Returns /proc/sys/kernel/random/boot_id on Linux.
   *
   * An empty value means that the boot identity could not be read.
   */
  static std::string currentBootId();

  static std::filesystem::path path();

  static std::filesystem::path path(
      const std::string& executionSlot);

  static bool save(
      const ClientExecutionState& state);

  static bool save(
      const ClientExecutionState& state,
      const std::string& executionSlot);

  static std::optional<ClientExecutionState>
  load();

  static std::optional<ClientExecutionState>
  load(const std::string& executionSlot);

  static bool remove();

  static bool remove(
      const std::string& executionSlot);
};

} // namespace openpuzzle::client
