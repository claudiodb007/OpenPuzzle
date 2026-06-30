#include "openpuzzle/core/Scheduler.hpp"

using namespace openpuzzle;

int main() {
    Scheduler scheduler;
    EventBus bus;

    int startedEvents = 0;
    int finishedEvents = 0;

    bus.subscribe([&](const Event& event) {
        if (event.type == EventType::ExecutionStarted) {
            startedEvents++;
        }

        if (event.type == EventType::ExecutionFinished) {
            finishedEvents++;
        }
    });

    ExecutionContext ctx;
    ctx.executionId = 1;
    ctx.jobId = 42;
    ctx.rangeId = 1001;

    ExecutionResult executionResult;
    executionResult.success = true;
    executionResult.exitCode = 0;

    auto result = scheduler.runOnceWithEvents(ctx, executionResult, bus);

    if (!result.success) return 1;
    if (startedEvents != 1) return 2;
    if (finishedEvents != 1) return 3;

    return 0;
}
