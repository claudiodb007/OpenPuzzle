#pragma once

namespace openpuzzle {

class WorkerRunService {
public:
    int run(bool dryRun, bool once) const;
};

} // namespace openpuzzle
