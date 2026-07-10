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

WorkerAgentState WorkerAgent::state() const {
  return info_.state;
}

bool WorkerAgent::offline() const {
  return info_.state == WorkerAgentState::Offline;
}

bool WorkerAgent::online() const {
  return !offline();
}

bool WorkerAgent::idle() const {
  return info_.state == WorkerAgentState::Idle;
}

bool WorkerAgent::busy() const {
  return info_.state == WorkerAgentState::Busy;
}

void WorkerAgent::markOffline() {
  info_.state = WorkerAgentState::Offline;
}

void WorkerAgent::markIdle() {
  info_.state = WorkerAgentState::Idle;
}

void WorkerAgent::markBusy() {
  info_.state = WorkerAgentState::Busy;
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

  record.id = info_.workerId;
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

std::string WorkerAgent::stateToString(WorkerAgentState state) {
  switch (state) {
  case WorkerAgentState::Offline:
    return "offline";
  case WorkerAgentState::Idle:
    return "idle";
  case WorkerAgentState::Busy:
    return "running";
  }

  return "offline";
}

WorkerAgentState WorkerAgent::stateFromString(const std::string& state) {
  if (state == "idle") {
    return WorkerAgentState::Idle;
  }

  if (state == "running" || state == "busy") {
    return WorkerAgentState::Busy;
  }

  return WorkerAgentState::Offline;
}

} // namespace openpuzzle
