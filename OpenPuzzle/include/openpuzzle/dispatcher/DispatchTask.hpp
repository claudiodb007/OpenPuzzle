#pragma once

#include "openpuzzle/models/Models.hpp"

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

    bool hasProfile = false;
    GpuProfileRecord profile;

    bool valid = false;
};

} // namespace openpuzzle
