#pragma once

#include <string>

namespace openpuzzle {

struct EngineRuntime {
    bool installed = false;

    std::string executable;
    std::string version;

    bool available = false;
};

} // namespace openpuzzle
