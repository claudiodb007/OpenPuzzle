#include "openpuzzle/core/ExecutionSession.hpp"

namespace openpuzzle {

std::string ExecutionSession::statusToString(ExecutionSessionStatus status) {
  switch (status) {
  case ExecutionSessionStatus::Created:
    return "CREATED";
  case ExecutionSessionStatus::Running:
    return "RUNNING";
  case ExecutionSessionStatus::Finished:
    return "FINISHED";
  case ExecutionSessionStatus::Failed:
    return "FAILED";
  case ExecutionSessionStatus::Cancelled:
    return "CANCELLED";
  }

  return "UNKNOWN";
}

} // namespace openpuzzle
