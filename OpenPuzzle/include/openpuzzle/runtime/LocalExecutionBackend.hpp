#pragma once

#include "openpuzzle/runtime/IExecutionBackend.hpp"

namespace openpuzzle {

class LocalExecutionBackend
    : public IExecutionBackend {
public:
    ExecutionHandle launch(
        const StartExecutionRequest& request) override;

    bool stop(
        const ExecutionHandle& handle) override;
};

} // namespace openpuzzle
