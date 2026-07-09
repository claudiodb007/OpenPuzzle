#pragma once

#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/runtime/StartExecutionRequest.hpp"

namespace openpuzzle {

class ExecutionRequestBuilder {
public:
  StartExecutionRequest build(const PuzzleRecord& puzzle,
                              const RangeRecord& range,
                              const JobRecord& job,
                              const std::string& executable,
                              const std::string& workspace) const;
};

} // namespace openpuzzle
