#pragma once

#include "openpuzzle/workers/WorkerState.hpp"

namespace openpuzzle {

class Worker {
public:
  explicit Worker(WorkerState state);

  const WorkerState& state() const;
  WorkerState& state();

  void markOnline();
  void markOffline();
  void markBusy();
  void markError();

  bool isOnline() const;
  bool isBusy() const;
  bool isOffline() const;

  bool supportsCuda() const;
  bool supportsOpenCL() const;
  bool supportsCpu() const;


private:
  WorkerState state_;
};

} // namespace openpuzzle
