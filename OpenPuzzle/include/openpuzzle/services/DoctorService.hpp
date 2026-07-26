#pragma once

#include <string>
#include <vector>

namespace openpuzzle {

class DoctorService {
public:
  int execute(
      const std::vector<std::string>& args) const;
};

} // namespace openpuzzle
