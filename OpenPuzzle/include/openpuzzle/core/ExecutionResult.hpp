#pragma once

#include <cstdint>
#include <string>

namespace openpuzzle {

struct ExecutionResult {
    bool success = false;

    int exitCode = -1;

    std::uint64_t linesRead = 0;

    double averageSpeed = 0.0;

    bool keyFound = false;

    std::string privateKey;
};

} // namespace openpuzzle
