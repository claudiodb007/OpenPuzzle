#include "openpuzzle/services/PuzzleService.hpp"
#include "openpuzzle/database/Database.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace openpuzzle {

PuzzleService::PuzzleService(Database& database) : database_(database) {}

int PuzzleService::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: OpenPuzzle puzzle list | show <number>\n";
        return 1;
    }

    if (args[0] == "list") {
        std::cout << "ID   STATUS   ADDRESS\n";
        std::cout << "--------------------------------------------------\n";

        for (auto& p : database_.listPuzzles()) {
            std::cout << std::setw(3) << p.number << "  "
                      << (p.solved ? "solved " : "open   ") << "  "
                      << p.address << "\n";
        }

        return 0;
    }

    if (args[0] == "show") {
        if (args.size() < 2) {
            throw std::runtime_error("Missing puzzle number");
        }

        int number = std::stoi(args[1]);
        auto p = database_.getPuzzleByNumber(number);

        if (!p) {
            throw std::runtime_error("Puzzle not found");
        }

        std::cout << "Puzzle............. " << p->number << "\n";
        std::cout << "Name............... " << p->name << "\n";
        std::cout << "Reward............. " << p->reward << " BTC\n";
        std::cout << "Status............. " << (p->solved ? "SOLVED" : "OPEN") << "\n";
        std::cout << "Address............ " << p->address << "\n";
        std::cout << "Hash160............ " << p->hash160 << "\n";
        std::cout << "Range Start........ " << p->rangeStart << "\n";
        std::cout << "Range End.......... " << p->rangeEnd << "\n";

        auto reserved = database_.countRangesByStatus(p->id, RangeStatus::Reserved);
        auto running = database_.countRangesByStatus(p->id, RangeStatus::Running);
        auto completed = database_.countRangesByStatus(p->id, RangeStatus::Completed);
        auto failed = database_.countRangesByStatus(p->id, RangeStatus::Failed);

        std::cout << "\n";
        std::cout << "Ranges reserved.... " << reserved << "\n";
        std::cout << "Ranges running..... " << running << "\n";
        std::cout << "Ranges completed... " << completed << "\n";
        std::cout << "Ranges failed...... " << failed << "\n";

        return 0;
    }

    std::cerr << "Unknown puzzle command\n";
    return 1;
}

} // namespace openpuzzle
