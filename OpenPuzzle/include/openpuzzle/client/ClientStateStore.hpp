#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"

#include <filesystem>
#include <optional>

namespace openpuzzle::client {

class ClientStateStore {
public:
  static std::filesystem::path path();

  static bool save(
      const ClientExecutionState& state);

  static std::optional<ClientExecutionState>
  load();

  static bool remove();
};

} // namespace openpuzzle::client
