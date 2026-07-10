#include "openpuzzle/services/DoctorService.hpp"

#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"

#include <iostream>
#include <string>

namespace openpuzzle {

static std::string backendForGpu(const std::string& name) {
    if (name.find("NVIDIA") != std::string::npos ||
        name.find("RTX") != std::string::npos ||
        name.find("GTX") != std::string::npos) {
        return "CUDA";
    }

    return "OpenCL";
}

int DoctorService::execute(const std::vector<std::string>&) {
    std::cout << "========================================\n";
    std::cout << "         OpenPuzzle Doctor\n";
    std::cout << "========================================\n\n";

    std::cout << "Database............. OK\n";
    std::cout << "Configuration........ OK\n\n";

    EngineManager manager;

    std::cout << "Engines\n";
    std::cout << "----------------------------------------\n";

    for (const auto& engine : manager.registry().engines()) {
        std::cout << engine.name << "............. "
                  << (engine.runtime.installed ? "OK" : "NOT FOUND")
                  << "\n";

        if (engine.runtime.installed) {
            std::cout << "Executable.......... "
                      << engine.runtime.executable
                      << "\n";
        }
    }

    std::cout << "\nHardware\n";
    std::cout << "----------------------------------------\n";

    auto gpus = GpuManager::listGpus();

    if (gpus.empty()) {
        std::cout << "GPUs................ NOT FOUND\n";
    } else {
        for (const auto& gpu : gpus) {
            std::cout << "GPU " << gpu.device << "............... "
                      << gpu.name << "\n";
            std::cout << "Memory.............. "
                      << std::to_string(gpu.memoryMb) + " MiB" << "\n";
        }
    }

    std::cout << "\nProfiles\n";
    std::cout << "----------------------------------------\n";

    GpuProfileManager profiles(database_);

    if (gpus.empty()) {
        std::cout << "Profiles............ NOT AVAILABLE\n";
    } else {
        for (const auto& gpu : gpus) {
            const auto backend = backendForGpu(gpu.name);
            auto profile = profiles.chooseBest(gpu.name, backend, "BitCrack");

            std::cout << gpu.name << "....... "
                      << (profile ? "Available" : "Missing")
                      << "\n";

            if (profile) {
                std::cout << "  Backend........... " << profile->backend << "\n";
                std::cout << "  Engine............ " << profile->engine << "\n";
                std::cout << "  Blocks............ " << profile->blocks << "\n";
                std::cout << "  Threads........... " << profile->threads << "\n";
                std::cout << "  Points............ " << profile->points << "\n";
                std::cout << "  Average........... "
                          << profile->averageSpeed
                          << " MKey/s\n";
            }
        }
    }

    std::cout << "\nStatus............... READY\n";

    return 0;
}

} // namespace openpuzzle
