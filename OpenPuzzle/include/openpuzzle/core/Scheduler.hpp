#pragma once

#include <string>

#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"
#include "openpuzzle/database/Database.hpp"

namespace openpuzzle {

struct SchedulerResult {
  bool success = false;
  int jobId = 0;
  int rangeId = 0;
  int exitCode = -1;
};

class Scheduler {
public:
  std::string workspaceForJob(int jobId) const;

  std::string buildBitCrackCommand(const std::string &bitcrackPath,
                                   const PuzzleRecord &puzzle,
                                   const RangeRecord &range, int device,
                                   int blocks, int threads, int points,
                                   const std::string &outputFile) const;

  ExecutionContext
  buildExecutionContext(int executionId, int puzzleId, int jobId, int rangeId,
                        const std::string &engine, const std::string &workspace,
                        const std::string &command, bool echoOutput) const;

  SchedulerResult runOnce(const ExecutionContext &context,
                          const ExecutionResult &executionResult) const;

  SchedulerResult runOnceWithEvents(const ExecutionContext &context,
                                    const ExecutionResult &executionResult,
                                    EventBus &bus) const;

  SchedulerResult runExecution(const ExecutionContext &context,
                               const ExecutionManager &executionManager) const;

  SchedulerResult runExistingJob(Database &db, const JobRecord &job,
                                 const RangeRecord &range,
                                 const ExecutionContext &context,
                                 const ExecutionManager &executionManager,
                                 bool dryRun) const;
};

} // namespace openpuzzle
