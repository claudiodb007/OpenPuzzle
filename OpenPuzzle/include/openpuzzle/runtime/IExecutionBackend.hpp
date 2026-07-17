#pragma once

#include "openpuzzle/runtime/ExecutionHandle.hpp"

namespace openpuzzle {

struct StartExecutionRequest;

class IExecutionBackend {
public:
    virtual ~IExecutionBackend() = default;

    virtual ExecutionHandle launch(
        const StartExecutionRequest& request) = 0;

    virtual bool stop(
        const ExecutionHandle& handle) = 0;
};

} // namespace openpuzzle
