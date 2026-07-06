#pragma once

#include "openpuzzle/engines/common/EngineRuntime.hpp"

#include <string>

namespace openpuzzle {

class EngineDiscovery {
public:
    EngineRuntime discover(const std::string& executable) const;
};

} // namespace openpuzzle
