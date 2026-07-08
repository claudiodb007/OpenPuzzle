#include "openpuzzle/services/EngineService.hpp"

#include "openpuzzle/engines/EngineManager.hpp"

#include <iomanip>
#include <iostream>

namespace openpuzzle {

int EngineService::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: OpenPuzzle engine list\n";
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

    std::cerr << "Unknown engine command\n";
    return 1;
}

} // namespace openpuzzle
