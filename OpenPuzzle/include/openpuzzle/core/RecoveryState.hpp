#pragma once

#include <cstdint>
#include <string>

namespace openpuzzle {

enum class RecoveryStatus {
    Unknown,
    Running,
    Finished,
    Failed
};

struct RecoveryState {
    int executionId = 0;
    int puzzleId = 0;
    int jobId = 0;
    int rangeId = 0;

    RecoveryStatus status = RecoveryStatus::Unknown;

    int exitCode = -1;

    std::uint64_t linesRead = 0;

    double averageSpeed = 0.0;

    bool keyFound = false;

    std::string privateKey;
};

} // namespace openpuzzle
