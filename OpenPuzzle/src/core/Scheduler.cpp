#include "openpuzzle/core/Scheduler.hpp"

namespace openpuzzle {

SchedulerResult
Scheduler::runOnce(const ExecutionContext &context,
                   const ExecutionResult &executionResult) const {
  SchedulerResult result;

  result.success = executionResult.success;
  result.jobId = context.jobId;
  result.rangeId = context.rangeId;
  result.exitCode = executionResult.exitCode;

  return result;
}

SchedulerResult
Scheduler::runOnceWithEvents(const ExecutionContext &context,
                             const ExecutionResult &executionResult,
                             EventBus &bus) const {
  bus.publish(Event{EventType::ExecutionStarted, context.executionId,
                    context.jobId, "Scheduler cycle started", "", 0.0});

  auto result = runOnce(context, executionResult);

  bus.publish(Event{
      result.success ? EventType::ExecutionFinished : EventType::Error,
      context.executionId, context.jobId,
      result.success ? "Scheduler cycle finished" : "Scheduler cycle failed",
      "", static_cast<double>(result.exitCode)});

  return result;
}

ExecutionContext Scheduler::buildExecutionContext(int executionId, int puzzleId,
                                                  int jobId, int rangeId,
                                                  const std::string &engine,
                                                  const std::string &workspace,
                                                  const std::string &command,
                                                  bool echoOutput) const {
  ExecutionContext context;

  context.executionId = executionId;
  context.puzzleId = puzzleId;
  context.jobId = jobId;
  context.rangeId = rangeId;
  context.engine = engine;
  context.workspace = workspace;
  context.command = command;
  context.echoOutput = echoOutput;

  return context;
}

SchedulerResult
Scheduler::runExecution(const ExecutionContext &context,
                        const ExecutionManager &executionManager) const {
  auto executionResult = executionManager.run(context);
  return runOnce(context, executionResult);
}

SchedulerResult Scheduler::runExistingJob(
    Database &db, const JobRecord &job, const RangeRecord &range,
    const ExecutionContext &context, const ExecutionManager &executionManager,
    bool dryRun) const {
  SchedulerResult schedulerResult;
  schedulerResult.jobId = job.id;
  schedulerResult.rangeId = range.id;

  int executionId =
      db.insertExecution(job.id, context.workspace, context.command,
                         dryRun ? "dry-run" : "running");

  if (executionId <= 0) {
    schedulerResult.success = false;
    schedulerResult.exitCode = -1;
    return schedulerResult;
  }

  if (dryRun) {
    schedulerResult.success = true;
    schedulerResult.exitCode = 0;
    return schedulerResult;
  }

  db.updateJobState(job.id, JobState::Running);
  db.updateRangeStatus(range.id, RangeStatus::Running);

  auto executionResult = executionManager.run(context);

  db.finishExecution(executionId,
                     executionResult.success ? "finished" : "failed",
                     executionResult.exitCode);

  if (executionResult.success) {
    db.updateJobState(job.id, JobState::Completed);
    db.updateRangeStatus(range.id, RangeStatus::Completed);
  } else {
    db.updateJobState(job.id, JobState::Failed);
    db.updateRangeStatus(range.id, RangeStatus::Failed);
  }

  schedulerResult.success = executionResult.success;
  schedulerResult.exitCode = executionResult.exitCode;

  return schedulerResult;
}

} // namespace openpuzzle
