#pragma once

#include "openpuzzle/engines/ISearchEngine.hpp"
#include "openpuzzle/engines/common/EngineFactory.hpp"
#include "openpuzzle/engines/common/EngineRegistry.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class EngineManager {
public:
  EngineManager();

  SearchEnginePtr create(
      const std::string& engine,
      const std::string& executable) const;

  std::optional<std::string> resolveExecutable(
      const std::string& engine,
      const std::string& backend) const;

  const EngineRegistry& registry() const;

private:
  static std::string normalize(
      std::string value);

  EngineRegistry registry_;
  EngineFactory factory_;
};

} // namespace openpuzzle
