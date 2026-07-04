#pragma once

#include <string>
#include <vector>

namespace openpuzzle {

class StartJobCommand {
public:
  int run(const std::vector<std::string> &args) const;
};

} // namespace openpuzzle
