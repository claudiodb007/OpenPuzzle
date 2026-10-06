#pragma once

#include <string>
#include <string_view>

namespace openpuzzle {

// Escape a JSON string's contents; callers supply its surrounding quotes.
inline std::string escapeJsonString(std::string_view value) {
  constexpr char hex[] = "0123456789abcdef";
  std::string escaped;
  for (const unsigned char character : value) {
    switch (character) {
    case '"': escaped += "\\\""; break;
    case '\\': escaped += "\\\\"; break;
    case '\b': escaped += "\\b"; break;
    case '\f': escaped += "\\f"; break;
    case '\n': escaped += "\\n"; break;
    case '\r': escaped += "\\r"; break;
    case '\t': escaped += "\\t"; break;
    default:
      if (character < 0x20) {
        escaped += "\\u00";
        escaped += hex[character >> 4];
        escaped += hex[character & 0x0f];
      } else {
        escaped += static_cast<char>(character);
      }
      break;
    }
  }
  return escaped;
}

} // namespace openpuzzle
