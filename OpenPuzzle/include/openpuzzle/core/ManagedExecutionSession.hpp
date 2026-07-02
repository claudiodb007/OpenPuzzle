#pragma once

#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ProcessHandle.hpp"

namespace openpuzzle {

class ManagedExecutionSession {
public:
  explicit ManagedExecutionSession(ExecutionContext context);

  const ExecutionContext &context() const;
  const ProcessHandle &handle() const;

  bool start();
  void finish(int exitCode);

  bool running() const;

private:
  ExecutionContext context_;
  ProcessHandle handle_;
};

} // namespace openpuzzle
