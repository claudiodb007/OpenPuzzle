#pragma once

#include <optional>

#include "openpuzzle/runtime/ExecutionHandle.hpp"

namespace openpuzzle {

struct StartExecutionRequest;

class IWorkerRuntime {
public:
    virtual ~IWorkerRuntime() = default;

    virtual bool start(
        const StartExecutionRequest& request) = 0;

    virtual bool stop() = 0;

    virtual bool complete() = 0;

    virtual bool busy() const = 0;

    virtual bool idle() const = 0;

    virtual const std::optional<ExecutionHandle>&
    currentExecution() const = 0;
};

} // namespace openpuzzle
