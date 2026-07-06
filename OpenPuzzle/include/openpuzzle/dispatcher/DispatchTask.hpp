#pragma once

#include <string>

namespace openpuzzle {

struct DispatchTask {
    int jobId = 0;
    int puzzleId = 0;
    int rangeId = 0;
    int workerId = 0;

    std::string engine;
    std::string backend;

    std::string rangeStart;
    std::string rangeEnd;

    bool valid = false;
};

} // namespace openpuzzle
