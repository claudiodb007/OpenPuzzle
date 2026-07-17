#include "openpuzzle/engines/common/EngineFactory.hpp"

#include <stdexcept>

namespace openpuzzle {

void EngineFactory::registerFactory(const std::string& id,
                                    EngineFactoryFunction factory) {
    factories_[id] = std::move(factory);
}

SearchEnginePtr EngineFactory::create(const std::string& id,
                                      const std::string& executable) const {
    auto it = factories_.find(id);

    if (it == factories_.end()) {
        throw std::runtime_error("Unknown engine: " + id);
    }

    return it->second(executable);
}

} // namespace openpuzzle
