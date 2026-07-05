#pragma once

#include "openpuzzle/models/Models.hpp"

#include <string>
#include <vector>

namespace openpuzzle::services {

std::string getArg(const std::vector<std::string>& args,
                   const std::string& name,
                   const std::string& def = {});

int getIntArg(const std::vector<std::string>& args,
              const std::string& name,
              int def);

std::string rangeStatusToString(RangeStatus status);

} // namespace openpuzzle::services
