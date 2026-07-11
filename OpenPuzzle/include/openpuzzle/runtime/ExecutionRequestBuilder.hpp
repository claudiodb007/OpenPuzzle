#pragma once

#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"
#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <string>

namespace openpuzzle {

class EngineManager;

class ExecutionRequestBuilder {
public:
  explicit ExecutionRequestBuilder(
      EngineManager& engineManager);

  StartExecutionRequest build(
      const PuzzleRecord& puzzle,
      const RangeRecord& range,
      const JobRecord& job,
      const WorkerEngineCapability& capability,
      const std::string& executable,
      const std::string& workspace) const;

private:
  EngineManager& engineManager_;
};

} // namespace openpuzzle
