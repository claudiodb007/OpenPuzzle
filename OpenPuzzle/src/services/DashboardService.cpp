#include "openpuzzle/services/DashboardService.hpp"

#include "openpuzzle/database/Database.hpp"

#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/services/ServiceUtils.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <iomanip>
#include <iostream>

namespace openpuzzle {

static const char* rangeStatusName(RangeStatus status) {
    switch (status) {
    case RangeStatus::Reserved:
        return "RESERVED";
    case RangeStatus::Running:
        return "RUNNING";
    case RangeStatus::Completed:
        return "COMPLETED";
    case RangeStatus::Failed:
        return "FAILED";
    case RangeStatus::Cancelled:
        return "CANCELLED";
    case RangeStatus::External:
        return "EXTERNAL";
    }

    return "UNKNOWN";
}

int DashboardService::execute(const std::vector<std::string>& args) {
    using namespace openpuzzle::services;

    int number = getIntArg(args, "--puzzle", 71);

    auto puzzle = database_.getPuzzleByNumber(number);

    if (!puzzle) {
        std::cerr << "Puzzle not found\n";
        return 1;
    }

    std::cout << "======================================\n";
    std::cout << "        OpenPuzzle Dashboard\n";
    std::cout << "======================================\n\n";

    std::cout << "Puzzle............... " << puzzle->name << "\n";
    std::cout << "Address.............. " << puzzle->address << "\n";
    std::cout << "Keyspace............. " << puzzle->rangeStart << ":"
              << puzzle->rangeEnd << "\n\n";

    std::cout << "Queue\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Ranges RESERVED...... "
              << database_.countRangesByStatus(puzzle->id, RangeStatus::Reserved)
              << "\n";
    std::cout << "Ranges RUNNING....... "
              << database_.countRangesByStatus(puzzle->id, RangeStatus::Running)
              << "\n";
    std::cout << "Ranges COMPLETED..... "
              << database_.countRangesByStatus(puzzle->id, RangeStatus::Completed)
              << "\n";
    std::cout << "Ranges FAILED........ "
              << database_.countRangesByStatus(puzzle->id, RangeStatus::Failed)
              << "\n";
    std::cout << "Jobs RESERVED........ "
              << database_.countJobsByState(puzzle->id, JobState::Reserved)
              << "\n";
    std::cout << "Jobs RUNNING......... "
              << database_.countJobsByState(puzzle->id, JobState::Running)
              << "\n";
    std::cout << "Jobs COMPLETED....... "
              << database_.countJobsByState(puzzle->id, JobState::Completed)
              << "\n\n";

    std::cout << "Workers\n";
    std::cout << "--------------------------------------\n";
    std::cout << "ID   MACHINE        GPU                    STATUS      SPEED\n";
    std::cout << "------------------------------------------------------------------\n";

    double totalSpeed = 0.0;

    for (const auto& worker : database_.listWorkers()) {
        if (worker.status == "running" || worker.status == "idle") {
            totalSpeed += worker.speedMkeys;
        }

        std::cout << std::setw(3) << worker.id << "  "
                  << std::setw(13) << worker.machine << "  "
                  << std::setw(22) << worker.gpuName << "  "
                  << std::setw(10) << worker.status << "  "
                  << worker.speedMkeys << " MKey/s\n";
    }

    std::cout << "\nPerformance\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Cluster Speed........ " << totalSpeed << " MKey/s\n\n";

    std::cout << "Recent ranges\n";
    std::cout << "--------------------------------------\n";

    auto ranges = database_.listRanges(puzzle->id);
    int shown = 0;

    for (const auto& range : ranges) {
        std::cout << "#" << range.id << " "
                  << range.startKey << ":"
                  << range.endKey << " "
                  << rangeStatusName(range.status)
                  << " block_bits=" << range.blockBits
                  << "\n";

        if (++shown >= 10)
            break;
    }

    auto bitcrack = ToolManager::bitcrackPath();

    std::cout << "\nConfiguration\n";
    std::cout << "--------------------------------------\n";
    std::cout << "BitCrack............. "
              << (bitcrack ? *bitcrack : "(not configured)") << "\n";
    std::cout << "Selected GPU......... " << GpuManager::selectedGpu() << "\n";

    return 0;
}

} // namespace openpuzzle
