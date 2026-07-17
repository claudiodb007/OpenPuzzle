#include "openpuzzle/services/DispatchService.hpp"
#include "openpuzzle/services/HeartbeatService.hpp"

#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/dispatcher/Dispatcher.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"

#include <iostream>
#include <stdexcept>

namespace openpuzzle {

DispatchServiceResult DispatchService::dispatch(CommandContext& context,
                                                bool dryRun) const {
    DispatchServiceResult serviceResult;

    if (!context.bitcrack) {
        std::cerr << "BitCrack is not configured\n";
        return serviceResult;
    }

    Dispatcher dispatcher(context.db);

    auto execution = dispatcher.nextExecution(
        context.scheduler,
        *context.bitcrack,
        context.gpu
    );

    if (!execution) {
        std::cout << "No dispatchable work available\n";
        serviceResult.success = true;
        serviceResult.workAvailable = false;
        return serviceResult;
    }

    auto job = context.db.getJob(execution->jobId);
    auto range = context.db.getRange(execution->rangeId);

    if (!job || !range) {
        throw std::runtime_error("Dispatch job/range not found");
    }

    std::cout << "Dispatching job\n";
    std::cout << "Job................ " << execution->jobId << "\n";
    std::cout << "Range.............. " << execution->rangeId << "\n";
    std::cout << "Engine............. " << execution->engine << "\n";
    std::cout << "Workspace.......... " << execution->workspace << "\n";
    std::cout << "Dry run............ " << (dryRun ? "yes" : "no") << "\n\n";
    std::cout << execution->command << "\n";

    HeartbeatService heartbeat(context.db);

    execution->onProgress = [&](const ExecutionResult& progress) {
        heartbeat.update(
            "escritorio",
            GpuManager::currentGpu().name,
            "CUDA",
            "BitCrack",
            "running",
            progress.averageSpeed,
            0.0,
            0.0);
    };

    ExecutionManager manager;

    auto result = context.scheduler.runExistingJob(
        context.db,
        *job,
        *range,
        *execution,
        manager,
        dryRun
    );

    serviceResult.success = result.success;
    serviceResult.workAvailable = true;
    serviceResult.jobId = result.jobId;
    serviceResult.rangeId = result.rangeId;
    serviceResult.executionId = result.executionId;
    serviceResult.exitCode = result.exitCode;

    return serviceResult;
}

} // namespace openpuzzle
