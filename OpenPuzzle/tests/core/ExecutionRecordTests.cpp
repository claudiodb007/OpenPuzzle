#include "openpuzzle/core/ExecutionRecord.hpp"

using namespace openpuzzle;

int main() {
  ExecutionRecord record;

  if (record.executionId != 0)
    return 1;
  if (record.status != ExecutionRecordStatus::Created)
    return 2;
  if (ExecutionRecord::statusToString(ExecutionRecordStatus::Running) !=
      "RUNNING")
    return 3;
  if (ExecutionRecord::statusToString(ExecutionRecordStatus::Finished) !=
      "FINISHED")
    return 4;

  return 0;
}
