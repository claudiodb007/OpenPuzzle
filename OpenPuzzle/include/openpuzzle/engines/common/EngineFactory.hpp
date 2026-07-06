#pragma once

#include "openpuzzle/engines/ISearchEngine.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace openpuzzle {

using EngineFactoryFunction =
    std::function<SearchEnginePtr(const std::string& executable)>;

class EngineFactory {
public:
    void registerFactory(const std::string& id,
                         EngineFactoryFunction factory);

    SearchEnginePtr create(const std::string& id,
                           const std::string& executable) const;

private:
    std::unordered_map<std::string, EngineFactoryFunction> factories_;
};

} // namespace openpuzzle
