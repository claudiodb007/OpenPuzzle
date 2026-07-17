#include "openpuzzle/core/commands/DispatchCommand.hpp"

#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/services/DispatchService.hpp"

#include <iostream>

namespace openpuzzle {

static bool hasArg(const std::vector<std::string>& args,
                   const std::string& name) {
    for (const auto& arg : args) {
        if (arg == name)
            return true;
    }
    return false;
}

int DispatchCommand::run(const std::vector<std::string>& args) const {
    const bool dryRun = hasArg(args, "--dry-run");

    CommandContext context;

    if (!context.initialize()) {
        std::cerr << context.lastError() << '\n';
        return 1;
    }

    DispatchService service;

    auto result = service.dispatch(context, dryRun);

    return result.success ? 0 : 1;
}

} // namespace openpuzzle
