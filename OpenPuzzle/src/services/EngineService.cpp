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

        std::cout << "ID          NAME        BACKEND      INSTALLED\n";
        std::cout << "------------------------------------------------\n";

        for (const auto& engine : manager.registry().engines()) {
            std::cout << std::left
                      << std::setw(11) << engine.id
                      << std::setw(12) << engine.name
                      << std::setw(13) << engine.backend
                      << (engine.runtime.installed ? "yes" : "no")
                      << "\n";
        }

        return 0;
    }

    std::cerr << "Unknown engine command\n";
    return 1;
}

} // namespace openpuzzle
