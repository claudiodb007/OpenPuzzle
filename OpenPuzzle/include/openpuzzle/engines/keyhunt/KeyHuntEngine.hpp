#pragma once

#include "openpuzzle/engines/ISearchEngine.hpp"

#include <string>

namespace openpuzzle {

class KeyHuntEngine : public ISearchEngine {
public:
    explicit KeyHuntEngine(std::string executable);

    EngineInfo info() const override;

    bool prepare() override;
    std::string buildCommand(const EngineLaunchRequest& request) const override;
    bool launch() override;
    bool stop() override;
    bool running() const override;

private:
    std::string executable_;
    bool running_ = false;
};

} // namespace openpuzzle
