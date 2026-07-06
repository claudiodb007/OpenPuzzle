#include "openpuzzle/engines/EngineManager.hpp"

#include "openpuzzle/engines/bitcrack/BitCrackEngine.hpp"
#include "openpuzzle/engines/common/EngineDiscovery.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

namespace openpuzzle {

EngineManager::EngineManager() {
    EngineDescriptor bitcrack;
    bitcrack.id = "bitcrack";
    bitcrack.name = "BitCrack";
    bitcrack.version = "unknown";
    bitcrack.backend = "CUDA/OpenCL";
    EngineDiscovery discovery;
    auto configuredBitCrack = ToolManager::bitcrackPath();
    bitcrack.runtime =
        discovery.discover(configuredBitCrack.value_or("cuBitCrack"));

    bitcrack.capabilities.cuda = true;
    bitcrack.capabilities.opencl = true;
    bitcrack.capabilities.cpu = false;
    bitcrack.capabilities.supportsCompressed = true;
    bitcrack.capabilities.supportsUncompressed = false;
    bitcrack.capabilities.supportsResume = true;
    bitcrack.capabilities.supportsCheckpoint = true;
    bitcrack.capabilities.supportsBenchmark = true;

    registry_.registerEngine(bitcrack);

    factory_.registerFactory("bitcrack", [](const std::string& executable) {
        return std::make_unique<BitCrackEngine>(executable);
    });
}

SearchEnginePtr EngineManager::create(const std::string& engine,
                                      const std::string& executable) const {
    return factory_.create(engine, executable);
}

const EngineRegistry& EngineManager::registry() const {
    return registry_;
}

} // namespace openpuzzle
