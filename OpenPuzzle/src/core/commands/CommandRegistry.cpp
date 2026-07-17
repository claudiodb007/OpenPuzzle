#include "openpuzzle/core/commands/CommandRegistry.hpp"

#include <stdexcept>

namespace openpuzzle {

void CommandRegistry::registerCommand(const std::string& name,
                                      CommandHandler handler) {
    handlers_[name] = std::move(handler);
}

bool CommandRegistry::has(const std::string& name) const {
    return handlers_.find(name) != handlers_.end();
}

int CommandRegistry::execute(const std::string& name,
                             const std::vector<std::string>& args) const {
    auto it = handlers_.find(name);

    if (it == handlers_.end()) {
        throw std::runtime_error("Unknown command: " + name);
    }

    return it->second(args);
}

} // namespace openpuzzle
