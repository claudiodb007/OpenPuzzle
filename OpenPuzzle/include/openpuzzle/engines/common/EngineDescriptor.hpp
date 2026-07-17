#pragma once

#include "openpuzzle/engines/common/EngineCapabilities.hpp"
#include "openpuzzle/engines/common/EngineRuntime.hpp"

#include <string>

namespace openpuzzle {

struct EngineDescriptor {
    std::string id;
    std::string name;
    std::string version;
    std::string backend;
    EngineRuntime runtime;

    EngineCapabilities capabilities;
};

} // namespace openpuzzle
