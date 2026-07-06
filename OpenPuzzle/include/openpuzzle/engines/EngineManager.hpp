#pragma once

#include "openpuzzle/engines/ISearchEngine.hpp"
#include "openpuzzle/engines/common/EngineFactory.hpp"
#include "openpuzzle/engines/common/EngineRegistry.hpp"

#include <string>

namespace openpuzzle {

class EngineManager {
public:
    EngineManager();

    SearchEnginePtr create(const std::string& engine,
                           const std::string& executable) const;

    const EngineRegistry& registry() const;

private:
    EngineRegistry registry_;
    EngineFactory factory_;
};

} // namespace openpuzzle
