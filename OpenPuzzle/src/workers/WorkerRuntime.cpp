#include "openpuzzle/workers/WorkerRuntime.hpp"

#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/workers/WorkerAgent.hpp"

#include <stdexcept>

namespace openpuzzle {

WorkerRuntime::WorkerRuntime(
    WorkerAgent& worker)
    : worker_(worker) {}

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

  BackgroundExecutionLauncher launcher;

  const auto handle =
      worker_.execute(
          launcher,
          request);

  return handle.pid > 0;
}

bool WorkerRuntime::stop() {
  if (!worker_.hasExecution()) {
    return false;
  }

  ExecutionStopper stopper;

  return worker_.stop(stopper);
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
