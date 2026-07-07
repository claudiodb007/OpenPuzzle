#include "openpuzzle/dispatcher/WorkerSelector.hpp"

#include "openpuzzle/database/Database.hpp"

namespace openpuzzle {

WorkerSelector::WorkerSelector(Database& database)
    : database_(database) {}

std::optional<WorkerRecord> WorkerSelector::selectIdleWorker() {

    auto workers = database_.listWorkers();

    for (const auto& worker : workers) {
        if (worker.status == "idle") {
            return worker;
        }
    }

    return std::nullopt;
}

} // namespace openpuzzle
