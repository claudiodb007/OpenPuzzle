#include "openpuzzle/engines/common/EngineRegistry.hpp"

namespace openpuzzle {

void EngineRegistry::registerEngine(const EngineDescriptor& engine) {
    engines_.push_back(engine);
}

const std::vector<EngineDescriptor>& EngineRegistry::engines() const {
    return engines_;
}

const EngineDescriptor* EngineRegistry::find(const std::string& id) const {
    for (const auto& engine : engines_) {
        if (engine.id == id) {
            return &engine;
        }
    }

    return nullptr;
}

} // namespace openpuzzle
