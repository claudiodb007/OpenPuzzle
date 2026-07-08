#pragma once

#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/runtime/ExecutionState.hpp"

namespace openpuzzle {

class Execution {
public:
  explicit Execution(ExecutionState state);

  const ExecutionState& state() const;
  ExecutionState& state();

  void start();
  void finish(const ExecutionResult& result);
  void fail(const ExecutionResult& result);
  void stop();

  bool isPending() const;
  bool isRunning() const;
  bool isFinished() const;
  bool isFailed() const;

private:
  ExecutionState state_;
};

} // namespace openpuzzle
