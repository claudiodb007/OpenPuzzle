#include "openpuzzle/services/WorkerService.hpp"
#include "openpuzzle/services/ServiceUtils.hpp"

#include "openpuzzle/database/Database.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace openpuzzle {

int WorkerService::execute(const std::vector<std::string>& args) {
    using namespace openpuzzle::services;

    if (args.empty()) {
        std::cerr << "Usage: OpenPuzzle worker register|list|show|enable|disable|drain\n";
        return 1;
    }

    if (args[0] == "register") {
        WorkerRecord w;
        w.machine = getArg(args, "--machine", "local");
        w.gpuName = getArg(args, "--gpu", "unknown");
        w.backend = getArg(args, "--backend", "unknown");
        w.engine = getArg(args, "--engine", "bitcrack");
        w.status = getArg(args, "--status", "idle");

        int id = database_.upsertWorker(w);

        std::cout << "Worker registered\n";
        std::cout << "Machine............ " << w.machine << "\n";
        std::cout << "GPU................ " << w.gpuName << "\n";
        std::cout << "Backend............ " << w.backend << "\n";
        std::cout << "Engine............. " << w.engine << "\n";
        std::cout << "Status............. " << w.status << "\n";

        if (id > 0) {
            std::cout << "ID................. " << id << "\n";
        }

        return 0;
    }

    if (args[0] == "list") {
        std::cout << "ID   MACHINE        GPU                    BACKEND   ENGINE      STATUS\n";
        std::cout << "----------------------------------------------------------------------------\n";

        for (auto& w : database_.listWorkers()) {
            std::cout << std::setw(3) << w.id << "  "
                      << std::setw(13) << w.machine << "  "
                      << std::setw(22) << w.gpuName << "  "
                      << std::setw(7) << w.backend << "  "
                      << std::setw(10) << w.engine << "  "
                      << w.status << "\n";
        }

        return 0;
    }

    if (args[0] == "show") {
        if (args.size() < 2) {
            throw std::runtime_error("Missing worker id");
        }

        int id = std::stoi(args[1]);
        auto w = database_.getWorker(id);

        if (!w) {
            throw std::runtime_error("Worker not found");
        }

        std::cout << "Worker............. " << w->id << "\n";
        std::cout << "Machine............ " << w->machine << "\n";
        std::cout << "GPU................ " << w->gpuName << "\n";
        std::cout << "Backend............ " << w->backend << "\n";
        std::cout << "Engine............. " << w->engine << "\n";
        std::cout << "Status............. " << w->status << "\n";
        std::cout << "Speed.............. " << w->speedMkeys << " MKey/s\n";
        std::cout << "Temperature........ " << w->temperature << " C\n";
        std::cout << "Power.............. " << w->power << " W\n";

        return 0;
    }

    if (args[0] == "enable" || args[0] == "disable" || args[0] == "drain") {
        if (args.size() < 2) {
            throw std::runtime_error("Missing worker id");
        }

        int id = std::stoi(args[1]);
        auto w = database_.getWorker(id);

        if (!w) {
            throw std::runtime_error("Worker not found");
        }

        if (args[0] == "enable") {
            w->status = "idle";
        } else if (args[0] == "disable") {
            w->status = "disabled";
        } else {
            w->status = "draining";
        }

        database_.upsertWorker(*w);

        std::cout << "Worker............. " << w->id << "\n";
        std::cout << "Status............. " << w->status << "\n";

        return 0;
    }

    std::cerr << "Unknown worker command\n";
    return 1;
}

} // namespace openpuzzle
