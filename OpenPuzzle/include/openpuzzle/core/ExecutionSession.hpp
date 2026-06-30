#pragma once

#include <string>

namespace openpuzzle {

enum class ExecutionSessionStatus {
    Created,
    Running,
    Finished,
    Failed,
    Cancelled
};

struct ExecutionSession {
    int executionId = 0;
    int jobId = 0;
    int puzzleId = 0;
    int rangeId = 0;

    std::string engine;
    std::string workspace;
    std::string command;

    ExecutionSessionStatus status = ExecutionSessionStatus::Created;

    static std::string statusToString(ExecutionSessionStatus status);
};

} // namespace openpuzzle
