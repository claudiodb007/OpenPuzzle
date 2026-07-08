#include "openpuzzle/services/EngineService.hpp"

#include "openpuzzle/engines/EngineManager.hpp"

#include <iomanip>
#include <iostream>

namespace openpuzzle {

static void printCapability(const std::string& label, bool enabled) {
    std::cout << "  " << std::left << std::setw(24) << label
              << (enabled ? "yes" : "no") << "\n";
}

int EngineService::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: OpenPuzzle engine list|info <id>\n";
        return 1;
    }

    if (args[0] == "list") {
        EngineManager manager;

        std::cout << "ID          NAME        BACKEND       INSTALLED   EXECUTABLE\n";
        std::cout << "--------------------------------------------------------------------------\n";

        for (const auto& engine : manager.registry().engines()) {
            std::cout << std::left
                      << std::setw(12) << engine.id
                      << std::setw(12) << engine.name
                      << std::setw(14) << engine.backend
                      << std::setw(12) << (engine.runtime.installed ? "yes" : "no")
                      << (engine.runtime.executable.empty() ? "-" : engine.runtime.executable)
                      << "\n";
        }

        return 0;
    }

    if (args[0] == "info") {
        if (args.size() < 2) {
            std::cerr << "Usage: OpenPuzzle engine info <id>\n";
            return 1;
        }

        EngineManager manager;
        const auto* engine = manager.registry().find(args[1]);

        if (!engine) {
            std::cerr << "Engine not found: " << args[1] << "\n";
            return 1;
        }

        std::cout << "Engine\n";
        std::cout << "------\n";
        std::cout << "ID................. " << engine->id << "\n";
        std::cout << "Name............... " << engine->name << "\n";
        std::cout << "Backend............ " << engine->backend << "\n";
        std::cout << "Version............ " << engine->version << "\n";

        std::cout << "\nRuntime\n";
        std::cout << "-------\n";
        std::cout << "Installed.......... " << (engine->runtime.installed ? "yes" : "no") << "\n";
        std::cout << "Available.......... " << (engine->runtime.available ? "yes" : "no") << "\n";
        std::cout << "Executable......... "
                  << (engine->runtime.executable.empty() ? "-" : engine->runtime.executable)
                  << "\n";
        std::cout << "Runtime version.... " << engine->runtime.version << "\n";

        std::cout << "\nCapabilities\n";
        std::cout << "------------\n";
        printCapability("CUDA", engine->capabilities.cuda);
        printCapability("OpenCL", engine->capabilities.opencl);
        printCapability("CPU", engine->capabilities.cpu);
        printCapability("Compressed", engine->capabilities.supportsCompressed);
        printCapability("Uncompressed", engine->capabilities.supportsUncompressed);
        printCapability("Resume", engine->capabilities.supportsResume);
        printCapability("Checkpoint", engine->capabilities.supportsCheckpoint);
        printCapability("Benchmark", engine->capabilities.supportsBenchmark);
        printCapability("Multiple targets", engine->capabilities.supportsMultipleTargets);
        printCapability("Distributed ready", engine->capabilities.distributedReady);

        std::cout << "\nDefaults\n";
        std::cout << "--------\n";
        std::cout << "Blocks............. " << engine->capabilities.defaultBlocks << "\n";
        std::cout << "Threads............ " << engine->capabilities.defaultThreads << "\n";
        std::cout << "Points............. " << engine->capabilities.defaultPoints << "\n";

        return 0;
    }

    std::cerr << "Unknown engine command\n";
    return 1;
}

} // namespace openpuzzle
