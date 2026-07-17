#pragma once

#include <string>
#include <vector>

namespace openpuzzle {

class DispatchCommand {
public:
    int run(const std::vector<std::string>& args) const;
};

} // namespace openpuzzle
