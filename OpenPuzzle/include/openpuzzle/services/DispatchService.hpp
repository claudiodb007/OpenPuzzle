#pragma once

namespace openpuzzle {

class CommandContext;

struct DispatchServiceResult {
    bool success = false;
    bool workAvailable = false;
    int jobId = 0;
    int rangeId = 0;
    int executionId = 0;
    int exitCode = -1;
};

class DispatchService {
public:
    DispatchServiceResult dispatch(CommandContext& context, bool dryRun) const;
};

} // namespace openpuzzle
