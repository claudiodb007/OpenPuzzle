#pragma once

#include <string>

namespace openpuzzle {

enum class SearchMode {
  Linear,
  Kangaroo
};

inline std::string toString(SearchMode mode) {
  return mode == SearchMode::Kangaroo ? "kangaroo" : "linear";
}

inline SearchMode searchModeFromString(const std::string &value) {
  return value == "kangaroo" ? SearchMode::Kangaroo : SearchMode::Linear;
}

} // namespace openpuzzle
