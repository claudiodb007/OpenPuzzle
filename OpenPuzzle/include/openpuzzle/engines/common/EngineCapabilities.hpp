#pragma once

#include <cstdint>

namespace openpuzzle {

struct EngineCapabilities {

    bool cuda = false;
    bool opencl = false;
    bool cpu = false;

    bool supportsCompressed = true;
    bool supportsUncompressed = true;

    bool supportsResume = true;
    bool supportsCheckpoint = true;
    bool supportsBenchmark = true;

    bool supportsMultipleTargets = false;
    bool distributedReady = false;

    uint32_t defaultBlocks = 256;
    uint32_t defaultThreads = 256;
    uint32_t defaultPoints = 1024;
};

} // namespace openpuzzle
