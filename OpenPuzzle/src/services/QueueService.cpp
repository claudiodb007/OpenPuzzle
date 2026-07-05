#include "openpuzzle/services/QueueService.hpp"
#include "openpuzzle/services/ServiceUtils.hpp"
#include "openpuzzle/allocator/RangeAllocator.hpp"
#include "openpuzzle/database/Database.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace openpuzzle {

QueueService::QueueService(Database& database) : database_(database) {}


int QueueService::execute(const std::vector<std::string>& args) {
    using namespace openpuzzle::services;
    if (args.empty()) {
        std::cerr << "Usage: OpenPuzzle queue add|list|show\n";
        return 1;
    }

    if (args[0] == "add") {
        int puzzleNumber = getIntArg(args, "--puzzle", 71);
        int blockBits = getIntArg(args, "--block-bits", 40);

        auto puzzle = database_.getPuzzleByNumber(puzzleNumber);
        if (!puzzle) {
            throw std::runtime_error("Puzzle not found");
        }

        RangeAllocator allocator(database_);
        auto range = allocator.allocateNext(*puzzle, blockBits);

        if (!range) {
            throw std::runtime_error("No range available");
        }

        JobRecord job;
        job.puzzleId = puzzle->id;
        job.rangeId = range->id;
        job.state = JobState::Reserved;
        job.id = database_.insertJob(job);

        std::cout << "Queued job.......... " << job.id << "\n";
        std::cout << "Puzzle.............. " << puzzle->number << "\n";
        std::cout << "Range ID............ " << range->id << "\n";
        std::cout << "Range Start......... " << range->startKey << "\n";
        std::cout << "Range End........... " << range->endKey << "\n";
        std::cout << "Block bits.......... " << blockBits << "\n";

        return 0;
    }

    if (args[0] == "list") {
        int puzzleNumber = getIntArg(args, "--puzzle", 71);

        auto puzzle = database_.getPuzzleByNumber(puzzleNumber);
        if (!puzzle) {
            throw std::runtime_error("Puzzle not found");
        }

        std::cout << "Queue for Puzzle " << puzzle->number << "\n";
        std::cout << "ID   STATUS      START                              END\n";
        std::cout << "-------------------------------------------------------------------------------\n";

        for (auto& range : database_.listRanges(puzzle->id)) {
            std::cout << std::setw(3) << range.id << "  "
                      << std::setw(10) << rangeStatusToString(range.status) << "  "
                      << std::setw(34) << range.startKey << "  "
                      << range.endKey << "\n";
        }

        return 0;
    }

    if (args[0] == "show") {
        if (args.size() < 2) {
            throw std::runtime_error("Missing range id");
        }

        int rangeId = std::stoi(args[1]);
        auto range = database_.getRange(rangeId);

        if (!range) {
            throw std::runtime_error("Range not found");
        }

        std::cout << "Range............... " << range->id << "\n";
        std::cout << "Puzzle ID........... " << range->puzzleId << "\n";
        std::cout << "Status.............. " << rangeStatusToString(range->status) << "\n";
        std::cout << "Start............... " << range->startKey << "\n";
        std::cout << "End................. " << range->endKey << "\n";
        std::cout << "Block bits.......... " << range->blockBits << "\n";
        std::cout << "Keys checked........ " << range->keysChecked << "\n";

        return 0;
    }

    std::cerr << "Unknown queue command\n";
    return 1;
}

} // namespace openpuzzle
