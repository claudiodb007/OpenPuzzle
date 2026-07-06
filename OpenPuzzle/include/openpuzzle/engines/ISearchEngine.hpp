#pragma once

#include <memory>
#include <string>

namespace openpuzzle {

struct EngineInfo {
    std::string name;
    std::string version;
    std::string backend;
    std::string executable;
    bool available = false;
};

class ISearchEngine {
public:
    virtual ~ISearchEngine() = default;

    virtual EngineInfo info() const = 0;

    virtual bool prepare() = 0;

    virtual bool launch() = 0;

    virtual bool stop() = 0;

    virtual bool running() const = 0;
};

using SearchEnginePtr = std::unique_ptr<ISearchEngine>;

} // namespace openpuzzle
