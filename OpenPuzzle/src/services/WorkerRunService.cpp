#include "openpuzzle/services/WorkerRunService.hpp"

#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/services/DispatchService.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace openpuzzle {

int WorkerRunService::run(bool dryRun, bool once) const {
    CommandContext context;

    if (!context.initialize()) {
        std::cerr << context.lastError() << '\n';
        return 1;
    }

    DispatchService dispatcher;

    std::cout << "OpenPuzzle Worker\n";
    std::cout << "=============================\n";
    std::cout << "Mode............... " << (once ? "once" : "daemon") << '\n';
    std::cout << "Dry run............ " << (dryRun ? "yes" : "no") << "\n\n";

    do {
        auto result = dispatcher.dispatch(context, dryRun);

        if (once)
            return result.success ? 0 : 1;

        if (!result.workAvailable) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

    } while (true);

    return 0;
}

} // namespace openpuzzle
