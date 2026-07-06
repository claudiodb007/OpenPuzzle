#pragma once

#include "openpuzzle/engines/common/EngineDescriptor.hpp"

#include <vector>

namespace openpuzzle {

class EngineRegistry {
public:
    void registerEngine(const EngineDescriptor& engine);

    const std::vector<EngineDescriptor>& engines() const;

    const EngineDescriptor* find(const std::string& id) const;

private:
    std::vector<EngineDescriptor> engines_;
};

} // namespace openpuzzle
