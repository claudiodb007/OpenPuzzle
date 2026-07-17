#pragma once

#include "openpuzzle/dispatcher/DispatchTask.hpp"
#include "openpuzzle/core/ExecutionContext.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class Database;
class Scheduler;

class Dispatcher {
public:
    explicit Dispatcher(Database& database);

    std::optional<DispatchTask> next();

    std::optional<ExecutionContext> nextExecution(
        Scheduler& scheduler,
        const std::string& bitcrackPath,
        int device
    );

private:
    Database& database_;
};

} // namespace openpuzzle
