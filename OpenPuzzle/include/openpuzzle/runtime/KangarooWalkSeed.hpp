#pragma once

#include <cerrno>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/random.h>

namespace openpuzzle {

class KangarooWalkSeed {
public:
  static bool valid(const std::string &value) {
    if (value.size() != 16)
      return false;

    bool nonZero = false;
    for (const unsigned char character : value) {
      if (std::isxdigit(character) == 0)
        return false;
      if (character != '0')
        nonZero = true;
    }

    return nonZero;
  }

  static std::string generate() {
    for (;;) {
      std::uint64_t value = 0;
      auto *bytes = reinterpret_cast<unsigned char *>(&value);
      std::size_t offset = 0;

      while (offset < sizeof(value)) {
        const auto received =
            ::getrandom(bytes + offset, sizeof(value) - offset, 0);

        if (received < 0 && errno == EINTR)
          continue;

        if (received <= 0)
          throw std::runtime_error(
              "Unable to obtain secure Kangaroo walk seed entropy");

        offset += static_cast<std::size_t>(received);
      }

      if (value == 0)
        continue;

      std::ostringstream formatted;
      formatted << std::uppercase << std::hex
                << std::setfill('0') << std::setw(16) << value;
      return formatted.str();
    }
  }
};

} // namespace openpuzzle
