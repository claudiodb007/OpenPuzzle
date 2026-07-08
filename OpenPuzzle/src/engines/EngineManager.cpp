#include "openpuzzle/engines/EngineManager.hpp"

#include "openpuzzle/engines/bitcrack/BitCrackEngine.hpp"
#include "openpuzzle/engines/keyhunt/KeyHuntEngine.hpp"
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


    EngineDescriptor keyhunt;
    keyhunt.id = "keyhunt";
    keyhunt.name = "KeyHunt";
    keyhunt.version = "unknown";
    keyhunt.backend = "CPU";

    keyhunt.runtime = discovery.discover("keyhunt");

    keyhunt.capabilities.cpu = true;
    keyhunt.capabilities.cuda = false;
    keyhunt.capabilities.opencl = false;
    keyhunt.capabilities.supportsCompressed = true;
    keyhunt.capabilities.supportsUncompressed = true;
    keyhunt.capabilities.supportsResume = false;
    keyhunt.capabilities.supportsCheckpoint = false;
    keyhunt.capabilities.supportsBenchmark = false;

    registry_.registerEngine(keyhunt);

    factory_.registerFactory("keyhunt", [](const std::string& executable) {
        return std::make_unique<KeyHuntEngine>(executable);
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
