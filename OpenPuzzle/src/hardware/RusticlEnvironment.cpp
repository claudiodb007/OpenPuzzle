#include "openpuzzle/hardware/RusticlEnvironment.hpp"

#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace openpuzzle {

bool RusticlEnvironment::valid(
    const std::string &selector) {
  if (selector.empty()) {
    return false;
  }

  for (const char character : selector) {
    const auto value =
        static_cast<unsigned char>(character);

    if (
        !std::isalnum(value) &&
        character != '_' &&
        character != '-' &&
        character != ',' &&
        character != ':') {
      return false;
    }
  }

  return true;
}

void RusticlEnvironment::apply(
    const std::string &selector) {
  if (!valid(selector)) {
    throw std::invalid_argument(
        "Invalid Rusticl device selector: " +
        selector);
  }

  if (
      setenv(
          "RUSTICL_ENABLE",
          selector.c_str(),
          1) != 0) {
    throw std::runtime_error(
        "Unable to configure RUSTICL_ENABLE");
  }
}

} // namespace openpuzzle
