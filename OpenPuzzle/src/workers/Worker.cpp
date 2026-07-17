#include "openpuzzle/workers/Worker.hpp"

namespace openpuzzle {

Worker::Worker(WorkerState state)
    : state_(std::move(state)) {}

const WorkerState& Worker::state() const {
  return state_;
}

WorkerState& Worker::state() {
  return state_;
}

void Worker::markOnline() {
  state_.status = WorkerStatus::Online;
}

void Worker::markOffline() {
  state_.status = WorkerStatus::Offline;
}

void Worker::markBusy() {
  state_.status = WorkerStatus::Busy;
}

void Worker::markError() {
  state_.status = WorkerStatus::Error;
}

bool Worker::isOnline() const {
  return state_.status == WorkerStatus::Online;
}

bool Worker::isBusy() const {
  return state_.status == WorkerStatus::Busy;
}

bool Worker::isOffline() const {
  return state_.status == WorkerStatus::Offline;
}


bool Worker::supportsCuda() const {
  return state_.capabilities.cuda;
}

bool Worker::supportsOpenCL() const {
  return state_.capabilities.opencl;
}

bool Worker::supportsCpu() const {
  return state_.capabilities.cpu;
}

} // namespace openpuzzle
