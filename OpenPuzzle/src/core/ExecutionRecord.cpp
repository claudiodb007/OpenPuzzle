#include "openpuzzle/core/ExecutionRecord.hpp"

namespace openpuzzle {

std::string ExecutionRecord::statusToString(ExecutionRecordStatus status) {
  switch (status) {
  case ExecutionRecordStatus::Created:
    return "CREATED";
  case ExecutionRecordStatus::Running:
    return "RUNNING";
  case ExecutionRecordStatus::Finished:
    return "FINISHED";
  case ExecutionRecordStatus::Failed:
    return "FAILED";
  case ExecutionRecordStatus::Cancelled:
    return "CANCELLED";
  }

  return "UNKNOWN";
}

} // namespace openpuzzle
