#include "openpuzzle/core/commands/WorkerRunCommand.hpp"

#include "openpuzzle/services/WorkerRunService.hpp"

#include <string>
#include <vector>

namespace openpuzzle {

static bool hasArg(const std::vector<std::string>& args,
                   const std::string& value) {
    for (const auto& arg : args) {
        if (arg == value)
            return true;
    }
    return false;
}

int WorkerRunCommand::run(const std::vector<std::string>& args) const {
    WorkerRunService service;

    return service.run(
        hasArg(args, "--dry-run"),
        hasArg(args, "--once"));
}

} // namespace openpuzzle
