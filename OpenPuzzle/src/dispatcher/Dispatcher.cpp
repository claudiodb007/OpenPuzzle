#include "openpuzzle/dispatcher/Dispatcher.hpp"

#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/dispatcher/ProfileSelector.hpp"
#include "openpuzzle/dispatcher/WorkerSelector.hpp"

#include <filesystem>

namespace openpuzzle {

Dispatcher::Dispatcher(Database& database)
    : database_(database) {}

std::optional<DispatchTask> Dispatcher::next() {
    auto job = database_.nextReservedJob();

    if (!job)
        return std::nullopt;

    auto range = database_.getRange(job->rangeId);

    if (!range)
        return std::nullopt;

    WorkerSelector workerSelector(database_);
    auto worker = workerSelector.selectIdleWorker();

    if (!worker)
        return std::nullopt;

    ProfileSelector profileSelector(database_);
    auto profile = profileSelector.select(*worker);

    DispatchTask task;

    task.jobId = job->id;
    task.puzzleId = job->puzzleId;
    task.rangeId = job->rangeId;

    task.workerId = worker->id;
    task.engine = worker->engine;
    task.backend = worker->backend;

    task.rangeStart = range->startKey;
    task.rangeEnd = range->endKey;

    if (profile) {
        task.hasProfile = true;
        task.profile = *profile;
    }

    task.valid = true;

    return task;
}

std::optional<ExecutionContext>
Dispatcher::nextExecution(Scheduler& scheduler,
                          const std::string& bitcrackPath,
                          int device) {

    auto task = next();

    if (!task)
        return std::nullopt;

    if (!task->hasProfile)
        return std::nullopt;

    auto puzzle = database_.getPuzzleById(task->puzzleId);

    if (!puzzle)
        return std::nullopt;

    auto range = database_.getRange(task->rangeId);

    if (!range)
        return std::nullopt;

    auto workspace = scheduler.workspaceForJob(task->jobId);

    auto outputFile =
        (std::filesystem::path(workspace) / "found.txt").string();

    auto logFile =
        (std::filesystem::path(workspace) / "bitcrack.log").string();

    auto command =
        scheduler.buildBitCrackCommand(
            bitcrackPath,
            *puzzle,
            *range,
            device,
            task->profile.blocks,
            task->profile.threads,
            task->profile.points,
            outputFile)
        + " 2>&1 | tee -a " + logFile;

    return scheduler.buildExecutionContext(
        0,
        task->puzzleId,
        task->jobId,
        task->rangeId,
        task->engine,
        workspace,
        command,
        true);
}

} // namespace openpuzzle
