#include "openpuzzle/workers/WorkerRuntime.hpp"

#include "openpuzzle/runtime/IExecutionBackend.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"

namespace openpuzzle {

WorkerRuntime::WorkerRuntime(
    WorkerAgent& worker,
    IExecutionBackend& backend)
    : worker_(worker),
      backend_(backend) {}

bool WorkerRuntime::start(
    const StartExecutionRequest& request) {
  if (!worker_.idle()) {
    return false;
  }

  if (request.executionId <= 0 ||
      request.jobId <= 0 ||
      request.command.empty() ||
      request.workspace.empty()) {
    return false;
  }

  if (!worker_.supports(
          request.engine,
          request.backend)) {
    return false;
  }

  const auto handle =
      backend_.launch(request);

  if (handle.pid <= 0) {
    return false;
  }

  if (!worker_.attachExecution(handle)) {
    backend_.stop(handle);
    return false;
  }

  return true;
}

bool WorkerRuntime::stop() {
  if (!worker_.currentExecution()) {
    return false;
  }

  const auto handle =
      *worker_.currentExecution();

  if (!backend_.stop(handle)) {
    return false;
  }

  return worker_.completeExecution();
}

bool WorkerRuntime::complete() {
  return worker_.completeExecution();
}

bool WorkerRuntime::busy() const {
  return worker_.busy() &&
         worker_.hasExecution();
}

bool WorkerRuntime::idle() const {
  return worker_.idle() &&
         !worker_.hasExecution();
}

const std::optional<ExecutionHandle>&
WorkerRuntime::currentExecution() const {
  return worker_.currentExecution();
}

} // namespace openpuzzle
