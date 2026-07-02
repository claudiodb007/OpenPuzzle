#include "openpuzzle/core/Scheduler.hpp"

#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace openpuzzle {

std::string Scheduler::workspaceForJob(int jobId) const {
  const char *home = std::getenv("HOME");
  std::filesystem::path path =
      home ? std::filesystem::path(home) : std::filesystem::current_path();

  std::ostringstream id;
  id << std::setw(6) << std::setfill('0') << jobId;

  path /= ".local/share/OpenPuzzle/jobs";
  path /= id.str();

  std::filesystem::create_directories(path);

  return path.string();
}

std::string Scheduler::buildBitCrackCommand(
    const std::string &bitcrackPath, const PuzzleRecord &puzzle,
    const RangeRecord &range, int device, int blocks, int threads, int points,
    const std::string &outputFile) const {
  std::ostringstream command;

  command << bitcrackPath << " " << puzzle.address << " --keyspace "
          << range.startKey << ":" << range.endKey << " --out " << outputFile
          << " -d " << device << " -b " << blocks << " -t " << threads << " -p "
          << points;

  return command.str();
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

SchedulerResult
Scheduler::runExecution(const ExecutionContext &context,
                        const ExecutionManager &executionManager) const {
  auto executionResult = executionManager.run(context);
  return runOnce(context, executionResult);
}

SchedulerResult Scheduler::startJob(Database &db, int puzzleNumber, int jobId,
                                    const std::string &bitcrackPath, int device,
                                    int blocks, int threads, int points,
                                    bool dryRun) const {
  auto puzzle = db.getPuzzleByNumber(puzzleNumber);
  auto job = db.getJob(jobId);

  if (!puzzle || !job) {
    SchedulerResult result;
    result.success = false;
    result.jobId = jobId;
    result.exitCode = -1;
    return result;
  }

  auto range = db.getRange(job->rangeId);

  if (!range) {
    SchedulerResult result;
    result.success = false;
    result.jobId = jobId;
    result.exitCode = -1;
    return result;
  }

  auto workspace = workspaceForJob(jobId);
  auto outputFile = (std::filesystem::path(workspace) / "found.txt").string();
  auto logFile = (std::filesystem::path(workspace) / "bitcrack.log").string();

  auto command = buildBitCrackCommand(bitcrackPath, *puzzle, *range, device,
                                      blocks, threads, points, outputFile) +
                 " 2>&1 | tee -a " + logFile;

  auto context = buildExecutionContext(0, puzzle->id, job->id, range->id,
                                       "BitCrack", workspace, command, true);

  ExecutionManager executionManager;
  return runExistingJob(db, *job, *range, context, executionManager, dryRun);
}

SchedulerResult
Scheduler::runExistingJob(Database &db, const JobRecord &job,
                          const RangeRecord &range, ExecutionContext context,
                          const ExecutionManager &executionManager,
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

  schedulerResult.executionId = executionId;

  if (dryRun) {
    schedulerResult.success = true;
    schedulerResult.exitCode = 0;
    return schedulerResult;
  }

  db.updateJobState(job.id, JobState::Running);
  db.updateRangeStatus(range.id, RangeStatus::Running);

  context.onProgress = [&](const ExecutionResult &progress) {
    if (!progress.keysChecked.empty()) {
      db.updateRangeKeysChecked(range.id, progress.keysChecked);
    }
  };

  auto executionResult = executionManager.run(context);

  if (!executionResult.keysChecked.empty()) {
    db.updateRangeKeysChecked(range.id, executionResult.keysChecked);
  }

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
