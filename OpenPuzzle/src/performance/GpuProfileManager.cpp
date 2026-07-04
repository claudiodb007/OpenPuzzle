#include "openpuzzle/performance/GpuProfileManager.hpp"

namespace openpuzzle {

GpuProfileManager::GpuProfileManager(Database &database)
    : database_(database) {}

bool GpuProfileManager::save(const GpuProfileRecord &profile) {
  return database_.saveGpuProfile(profile);
}

std::optional<GpuProfileRecord>
GpuProfileManager::load(const std::string &gpuName, const std::string &backend,
                        const std::string &engine) {
  return database_.getGpuProfile(gpuName, backend, engine);
}

} // namespace openpuzzle
