#include "openpuzzle/core/ExecutionManager.hpp"

#include <iostream>

namespace openpuzzle {

ExecutionSummary ExecutionManager::runCommand(const std::string& command, bool echoOutput) const {
    ExecutionSummary summary;

    ProcessRunner runner;
    bitcrack::BitCrackOutputParser parser;

    auto result = runner.run(command, [&](const std::string& line) {
        summary.totalLines++;

        if (echoOutput) {
            std::cout << line << "\n";
        }

        auto parsed = parser.parse(line);

        switch (parsed.type) {
            case bitcrack::ParsedLineType::Speed:
                summary.speedEvents++;
                summary.lastSpeedMKeys = parsed.speedMKeys;
                break;

            case bitcrack::ParsedLineType::Error:
                summary.errorEvents++;
                break;

            case bitcrack::ParsedLineType::Found:
                summary.foundEvents++;
                break;

            case bitcrack::ParsedLineType::Finished:
                summary.finishedEvents++;
                break;

            default:
                break;
        }
    });

    summary.started = result.started;
    summary.exitCode = result.exitCode;

    return summary;
}

} // namespace openpuzzle
