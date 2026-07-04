#pragma once

#include <string>
#include <vector>

namespace openpuzzle {

class BenchmarkCommand {
public:
  int run(const std::vector<std::string> &args) const;
};

} // namespace openpuzzle
