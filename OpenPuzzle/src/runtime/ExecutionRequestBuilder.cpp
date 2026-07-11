#include "openpuzzle/runtime/ExecutionRequestBuilder.hpp"

#include "openpuzzle/engines/EngineLaunchBuilder.hpp"
#include "openpuzzle/engines/EngineManager.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace openpuzzle {

static std::string normalizeEngineId(
    std::string engine) {
  std::transform(
      engine.begin(),
      engine.end(),
      engine.begin(),
      [](unsigned char character) {
        return static_cast<char>(
            std::tolower(character));
      });

  return engine;
}

ExecutionRequestBuilder::ExecutionRequestBuilder(
    EngineManager& engineManager)
    : engineManager_(engineManager) {}

StartExecutionRequest ExecutionRequestBuilder::build(
    const PuzzleRecord& puzzle,
    const RangeRecord& range,
    const JobRecord& job,
    const WorkerEngineCapability& capability,
    const std::string& executable,
    const std::string& workspace) const {
  const std::string engineId =
      normalizeEngineId(
          capability.engine);

  EngineLaunchBuilder launchBuilder;

  const auto launchRequest =
      launchBuilder.build(
          puzzle,
          range,
          job,
          capability,
          workspace);

  auto engine = engineManager_.create(
      engineId,
      executable);

  if (!engine) {
    throw std::runtime_error(
        "Could not create search engine: " +
        capability.engine);
  }

  StartExecutionRequest request;

  request.puzzleId = puzzle.id;
  request.jobId = job.id;
  request.rangeId = range.id;

  request.engine = capability.engine;
  request.backend = capability.backend;

  request.device = capability.device;
  request.blocks = capability.blocks;
  request.threads = capability.threads;
  request.points = capability.points;

  request.workspace = workspace;

  request.command =
      engine->buildCommand(
          launchRequest);

  request.echoOutput = true;

  return request;
}

} // namespace openpuzzle
