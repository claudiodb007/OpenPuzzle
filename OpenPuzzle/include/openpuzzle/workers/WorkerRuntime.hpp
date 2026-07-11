#pragma once

#include "openpuzzle/runtime/ExecutionHandle.hpp"
#include "openpuzzle/workers/IWorkerRuntime.hpp"

#include <optional>

namespace openpuzzle {

class BackgroundExecutionLauncher;
class ExecutionStopper;
class WorkerAgent;
struct StartExecutionRequest;

class WorkerRuntime : public IWorkerRuntime {
public:
  explicit WorkerRuntime(
      WorkerAgent& worker);

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
};

} // namespace openpuzzle
