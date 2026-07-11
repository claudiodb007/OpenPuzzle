#pragma once

#include "openpuzzle/runtime/ExecutionHandle.hpp"

#include <optional>

namespace openpuzzle {

class BackgroundExecutionLauncher;
class ExecutionStopper;
class WorkerAgent;
struct StartExecutionRequest;

class WorkerRuntime {
public:
  explicit WorkerRuntime(
      WorkerAgent& worker);

  bool start(
      const StartExecutionRequest& request);

  bool stop();

  bool complete();

  bool busy() const;
  bool idle() const;

  const std::optional<ExecutionHandle>&
  currentExecution() const;

private:
  WorkerAgent& worker_;
};

} // namespace openpuzzle
