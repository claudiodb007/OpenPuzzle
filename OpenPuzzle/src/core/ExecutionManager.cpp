#include "openpuzzle/core/ExecutionManager.hpp"

#include <filesystem>
#include <fstream>
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

ExecutionResult ExecutionManager::run(const ExecutionContext& context) const {
    ExecutionResult result;

    std::ofstream stdoutLog;

    if (!context.workspace.empty()) {
        std::filesystem::create_directories(context.workspace);
        stdoutLog.open(std::filesystem::path(context.workspace) / "stdout.log", std::ios::app);
    }

    ProcessRunner runner;
    bitcrack::BitCrackOutputParser parser;

    auto processResult = runner.run(context.command, [&](const std::string& line) {
        result.linesRead++;

        if (context.echoOutput) {
            std::cout << line << "\n";
        }

        if (stdoutLog.is_open()) {
            stdoutLog << line << "\n";
        }

        auto parsed = parser.parse(line);

        if (parsed.type == bitcrack::ParsedLineType::Speed) {
            result.averageSpeed = parsed.speedMKeys;
        }

        if (parsed.type == bitcrack::ParsedLineType::Found) {
            result.keyFound = true;
            result.privateKey = parsed.value;
        }
    });

    result.exitCode = processResult.exitCode;
    result.success = processResult.started && processResult.exitCode == 0;

    if (!context.workspace.empty()) {
        std::ofstream executionFile(std::filesystem::path(context.workspace) / "execution.json");

        if (executionFile.is_open()) {
            executionFile << "{\n";
            executionFile << "  \"execution_id\": " << context.executionId << ",\n";
            executionFile << "  \"puzzle_id\": " << context.puzzleId << ",\n";
            executionFile << "  \"job_id\": " << context.jobId << ",\n";
            executionFile << "  \"range_id\": " << context.rangeId << ",\n";
            executionFile << "  \"engine\": \"" << context.engine << "\",\n";
            executionFile << "  \"exit_code\": " << result.exitCode << ",\n";
            executionFile << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
            executionFile << "  \"lines_read\": " << result.linesRead << ",\n";
            executionFile << "  \"average_speed\": " << result.averageSpeed << ",\n";
            executionFile << "  \"key_found\": " << (result.keyFound ? "true" : "false") << "\n";
            executionFile << "}\n";
        }
    }

    return result;
}

} // namespace openpuzzle
