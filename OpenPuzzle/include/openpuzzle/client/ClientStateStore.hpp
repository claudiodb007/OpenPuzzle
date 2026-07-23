#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace openpuzzle::client {

class ClientStateStore {
public:
  static std::string executionSlot();

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
