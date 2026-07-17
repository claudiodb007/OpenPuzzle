#pragma once

#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/workers/WorkerEngineCapability.hpp"

#include <string>

namespace openpuzzle {

class EngineLaunchBuilder {
public:
  EngineLaunchRequest build(
      const PuzzleRecord& puzzle,
      const RangeRecord& range,
      const JobRecord& job,
      const WorkerEngineCapability& capability,
      const std::string& workspace) const;
};

} // namespace openpuzzle
