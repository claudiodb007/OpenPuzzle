#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace openpuzzle {

// Shortest decimal representation that round-trips to the same finite double.
// Unlike streams, this is independent of locale and preserves simple values
// such as 1334.62 without adding binary floating-point rounding digits.
inline std::string encodeJsonDouble(double value) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument("JSON numbers must be finite");
  }
  std::array<char, 64> buffer{};
  const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (converted.ec != std::errc{}) {
    throw std::runtime_error("Unable to encode JSON number");
  }
  return {buffer.data(), converted.ptr};
}

} // namespace openpuzzle
