#pragma once

#include "openpuzzle/models/Models.hpp"

#include <optional>

namespace openpuzzle {

class Database;

class WorkerSelector {
public:
    explicit WorkerSelector(Database& database);

    std::optional<WorkerRecord> selectIdleWorker();

private:
    Database& database_;
};

} // namespace openpuzzle
