#pragma once

#include "openpuzzle/dispatcher/DispatchTask.hpp"

#include <optional>

namespace openpuzzle {

class Database;

class Dispatcher {
public:
    explicit Dispatcher(Database& database);

    std::optional<DispatchTask> next();

private:
    Database& database_;
};

} // namespace openpuzzle
