#pragma once

#include "openpuzzle/models/Models.hpp"

#include <optional>

namespace openpuzzle {

class Database;

class ProfileSelector {
public:
    explicit ProfileSelector(Database& database);

    std::optional<GpuProfileRecord> select(const WorkerRecord& worker);

private:
    Database& database_;
};

} // namespace openpuzzle
