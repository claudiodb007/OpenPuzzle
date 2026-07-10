#pragma once

#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/workers/WorkerEngineCapability.hpp"

namespace openpuzzle {

class ExecutionRequestBuilder {
public:
  StartExecutionRequest build(
      const PuzzleRecord& puzzle,
      const RangeRecord& range,
      const JobRecord& job,
      const WorkerEngineCapability& capability,
      const std::string& executable,
      const std::string& workspace) const;
};

} // namespace openpuzzle
