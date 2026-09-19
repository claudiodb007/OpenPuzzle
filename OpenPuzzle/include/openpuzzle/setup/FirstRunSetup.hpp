#pragma once

#include <string>

namespace openpuzzle {

class FirstRunSetup {
public:
  bool ensureConfigured() const;

  bool ensureConfigured(
      const std::string& backend) const;
};

} // namespace openpuzzle
