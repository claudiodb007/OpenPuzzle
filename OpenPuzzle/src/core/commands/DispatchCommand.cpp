#include "openpuzzle/core/commands/DispatchCommand.hpp"

#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/dispatcher/Dispatcher.hpp"

#include <iostream>
#include <stdexcept>

namespace openpuzzle {

static bool hasArg(const std::vector<std::string>& args,
                   const std::string& name) {
    for (const auto& arg : args) {
        if (arg == name) {
            return true;
        }
    }

    return false;
}

int DispatchCommand::run(const std::vector<std::string>& args) const {
    const bool dryRun = hasArg(args, "--dry-run");

    CommandContext context;

    if (!context.initialize()) {
        std::cerr << context.lastError() << "\n";
        return 1;
    }

    if (!context.bitcrack) {
        std::cerr << "BitCrack is not configured\n";
        return 1;
    }

    Dispatcher dispatcher(context.db);

    auto execution = dispatcher.nextExecution(
        context.scheduler,
        *context.bitcrack,
        context.gpu
    );

    if (!execution) {
        std::cout << "No dispatchable work available\n";
        return 0;
    }

    auto job = context.db.getJob(execution->jobId);
    auto range = context.db.getRange(execution->rangeId);

    if (!job || !range) {
        throw std::runtime_error("Dispatch job/range not found");
    }

    std::cout << "Dispatching job\n";
    std::cout << "Job................ " << execution->jobId << "\n";
    std::cout << "Range.............. " << execution->rangeId << "\n";
    std::cout << "Engine............. " << execution->engine << "\n";
    std::cout << "Workspace.......... " << execution->workspace << "\n";
    std::cout << "Dry run............ " << (dryRun ? "yes" : "no") << "\n\n";
    std::cout << execution->command << "\n";

    ExecutionManager manager;

    auto result = context.scheduler.runExistingJob(
        context.db,
        *job,
        *range,
        *execution,
        manager,
        dryRun
    );

    return result.success ? 0 : 1;
}

} // namespace openpuzzle
