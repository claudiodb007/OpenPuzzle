#pragma once

#include "openpuzzle/engines/common/EngineDescriptor.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace openpuzzle {

class EngineRegistry {
public:
  void registerEngine(
      const EngineDescriptor& engine);

  const std::vector<EngineDescriptor>&
  engines() const;

  const EngineDescriptor* find(
      const std::string& id) const;

  std::vector<EngineDescriptor> installed() const;
  std::vector<EngineDescriptor> available() const;

  std::vector<EngineDescriptor> supportingBackend(
      const std::string& backend,
      bool installedOnly = false) const;

  std::size_t count() const;
  void clear();

private:
  static std::string normalizeId(
      std::string value);

  static bool supportsBackend(
      const EngineDescriptor& engine,
      const std::string& backend);

  std::vector<EngineDescriptor> engines_;
};

} // namespace openpuzzle
