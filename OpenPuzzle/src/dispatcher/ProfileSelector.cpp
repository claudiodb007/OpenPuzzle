#include "openpuzzle/dispatcher/ProfileSelector.hpp"

#include "openpuzzle/performance/GpuProfileManager.hpp"

namespace openpuzzle {

ProfileSelector::ProfileSelector(Database& database)
    : database_(database) {}

std::optional<GpuProfileRecord> ProfileSelector::select(const WorkerRecord& worker) {
    GpuProfileManager profiles(database_);

    return profiles.chooseBest(
        worker.gpuName,
        worker.backend,
        worker.engine
    );
}

} // namespace openpuzzle
