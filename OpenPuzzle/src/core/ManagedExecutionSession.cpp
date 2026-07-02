#include "openpuzzle/core/ManagedExecutionSession.hpp"

namespace openpuzzle {

ManagedExecutionSession::ManagedExecutionSession(ExecutionContext context)
    : context_(std::move(context)) {}

const ExecutionContext &ManagedExecutionSession::context() const {
  return context_;
}

const ProcessHandle &ManagedExecutionSession::handle() const { return handle_; }

bool ManagedExecutionSession::running() const { return handle_.running; }

} // namespace openpuzzle
