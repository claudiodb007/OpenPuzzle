#pragma once

#include <charconv>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace openpuzzle {

// Defaults apply only when the option is absent. Aliases name one option,
// so repeating either spelling is ambiguous and must not silently win.
inline int cliIntegerArgument(
    const std::vector<std::string>& args,
    std::initializer_list<std::string_view> names,
    int fallback) {
  bool found = false;
  int result = fallback;
  for (std::size_t index = 0; index < args.size(); ++index) {
    bool matches = false;
    for (const auto name : names) {
      matches = matches || args[index] == name;
    }
    if (!matches) continue;

    const auto& name = args[index];
    if (found) {
      throw std::runtime_error("Repeated integer option: " + name);
    }
    found = true;
    if (index + 1 == args.size() || args[index + 1].empty()) {
      throw std::runtime_error("Missing value for integer option: " + name);
    }

    std::string_view value = args[++index];
    // Retain explicitly signed decimal integers, without accepting whitespace,
    // decimal fractions, exponent notation or a numeric prefix of other text.
    if (value.front() == '+') {
      value.remove_prefix(1);
      if (value.empty() || value.front() == '-' || value.front() == '+') {
        throw std::runtime_error("Invalid integer value for " + name);
      }
    }
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
      throw std::runtime_error(
          "Invalid integer value for " + name + ": expected a whole decimal number within the int range");
    }
  }
  return result;
}

} // namespace openpuzzle
