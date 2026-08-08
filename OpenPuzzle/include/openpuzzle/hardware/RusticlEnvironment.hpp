#pragma once

#include <string>

namespace openpuzzle {

class RusticlEnvironment {
public:
  static bool valid(const std::string &selector);
  static void apply(const std::string &selector);
};

} // namespace openpuzzle
