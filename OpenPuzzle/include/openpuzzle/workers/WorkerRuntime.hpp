#pragma once

#include "openpuzzle/runtime/ExecutionHandle.hpp"
#include "openpuzzle/workers/IWorkerRuntime.hpp"

#include <optional>

namespace openpuzzle {

class IExecutionBackend;
class WorkerAgent;
struct StartExecutionRequest;

class WorkerRuntime final
    : public IWorkerRuntime {
public:
  WorkerRuntime(
      WorkerAgent& worker,
      IExecutionBackend& backend);

  bool start(
      const StartExecutionRequest& request) override;

  bool stop() override;
  bool complete() override;

  bool busy() const override;
  bool idle() const override;

  const std::optional<ExecutionHandle>&
  currentExecution() const override;

private:
  WorkerAgent& worker_;
  IExecutionBackend& backend_;
};

} // namespace openpuzzle
