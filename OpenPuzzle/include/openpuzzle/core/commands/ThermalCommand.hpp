#pragma once

#include <string>
#include <vector>

namespace openpuzzle {

class ThermalCommand {
public:
  int run(const std::vector<std::string> &args) const;
};

} // namespace openpuzzle
