#include "openpuzzle/engines/common/EngineDiscovery.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace openpuzzle {

EngineRuntime EngineDiscovery::discover(const std::string& executable) const {
    EngineRuntime runtime;

    runtime.executable = executable;
    runtime.installed = !executable.empty() && fs::exists(executable);
    runtime.available = runtime.installed;
    runtime.version = "unknown";

    return runtime;
}

} // namespace openpuzzle
