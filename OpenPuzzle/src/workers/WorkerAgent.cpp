#include "openpuzzle/workers/WorkerAgent.hpp"

#include "openpuzzle/runtime/BackgroundExecutionLauncher.hpp"
#include "openpuzzle/runtime/ExecutionStopper.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

#include <stdexcept>
#include <utility>

namespace openpuzzle {

WorkerAgent::WorkerAgent() = default;

WorkerAgent::WorkerAgent(WorkerAgentInfo info)
    : info_(std::move(info)) {}

const WorkerAgentInfo& WorkerAgent::info() const {
  return info_;
}

WorkerState WorkerAgent::state() const {
  return info_.state;
}

bool WorkerAgent::offline() const {
  return info_.state == WorkerState::Offline;
}

bool WorkerAgent::online() const {
  return !offline();
}

bool WorkerAgent::idle() const {
  return info_.state == WorkerState::Idle;
}

bool WorkerAgent::busy() const {
  return info_.state == WorkerState::Busy;
}

void WorkerAgent::markOffline() {
  info_.state = WorkerState::Offline;
}

void WorkerAgent::markIdle() {
  info_.state = WorkerState::Idle;
}

void WorkerAgent::markBusy() {
  info_.state = WorkerState::Busy;
}

bool WorkerAgent::hasExecution() const {
  return currentExecution_.has_value();
}

const std::optional<ExecutionHandle>&
WorkerAgent::currentExecution() const {
  return currentExecution_;
}

ExecutionHandle WorkerAgent::execute(
    BackgroundExecutionLauncher& launcher,
    const StartExecutionRequest& request) {
  if (offline()) {
    throw std::runtime_error("Offline worker cannot execute work");
  }

  if (hasExecution()) {
    throw std::runtime_error("Worker already has an active execution");
  }

  markBusy();

  try {
    auto handle = launcher.start(request);
    currentExecution_ = handle;
    return handle;
  } catch (...) {
    markIdle();
    throw;
  }
}

bool WorkerAgent::stop(ExecutionStopper& stopper) {
  if (!currentExecution_) {
    return false;
  }

  if (!stopper.stop(currentExecution_->workspace)) {
    return false;
  }

  currentExecution_.reset();
  markIdle();

  return true;
}

WorkerRecord WorkerAgent::toRecord() const {
  WorkerRecord record;

  record.machine = info_.machine;
  record.gpuName = info_.gpuName;
  record.backend = info_.backend;
  record.engine = info_.engine;
  record.status = stateToString(info_.state);
  record.speedMkeys = info_.speedMkeys;
  record.temperature = info_.temperature;
  record.power = info_.power;

  return record;
}

std::string WorkerAgent::stateToString(WorkerState state) {
  switch (state) {
  case WorkerState::Offline:
    return "offline";
  case WorkerState::Idle:
    return "idle";
  case WorkerState::Busy:
    return "running";
  }

  return "offline";
}

WorkerState WorkerAgent::stateFromString(const std::string& state) {
  if (state == "idle") {
    return WorkerState::Idle;
  }

  if (state == "running" || state == "busy") {
    return WorkerState::Busy;
  }

  return WorkerState::Offline;
}

} // namespace openpuzzle
