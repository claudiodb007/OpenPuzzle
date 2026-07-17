#include "openpuzzle/dispatcher/Dispatcher.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/database/Database.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

using namespace openpuzzle;

static bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

int main() {
    const char* dbPath = "/tmp/openpuzzle_dispatcher_execution_test.db";
    std::remove(dbPath);

    Database db;
    assert(db.open(dbPath));
    assert(db.createSchema());

    PuzzleRecord puzzle;
    puzzle.number = 71;
    puzzle.name = "Puzzle 71";
    puzzle.address = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
    puzzle.hash160 = "f6f5431d25bbf7b12e8add9af5e3475c44a0a5b8";
    puzzle.rangeStart = "400000000000000000";
    puzzle.rangeEnd = "7fffffffffffffffff";
    puzzle.reward = 71.0;
    puzzle.solved = false;
    puzzle.sharing = "public";

    assert(db.upsertPuzzle(puzzle));

    auto storedPuzzle = db.getPuzzleByNumber(71);
    assert(storedPuzzle);

    RangeRecord range;
    range.puzzleId = storedPuzzle->id;
    range.startKey = "400000000000000000";
    range.endKey = "40000000FFFFFFFFFF";
    range.blockBits = 40;
    range.status = RangeStatus::Reserved;

    int rangeId = db.insertRange(range);
    assert(rangeId > 0);

    JobRecord job;
    job.puzzleId = storedPuzzle->id;
    job.rangeId = rangeId;
    job.state = JobState::Reserved;

    int jobId = db.insertJob(job);
    assert(jobId > 0);

    WorkerRecord worker;
    worker.machine = "test-machine";
    worker.gpuName = "RTX 4070 Super";
    worker.backend = "CUDA";
    worker.engine = "BitCrack";
    worker.status = "idle";

    int workerId = db.upsertWorker(worker);
    assert(workerId >= 0);

    GpuProfileRecord profile;
    profile.gpuName = "RTX 4070 Super";
    profile.backend = "CUDA";
    profile.engine = "BitCrack";
    profile.blocks = 256;
    profile.threads = 512;
    profile.points = 2048;
    profile.averageSpeed = 1350.0;
    profile.minimumSpeed = 1300.0;
    profile.maximumSpeed = 1400.0;
    profile.samples = 6;

    assert(db.saveGpuProfile(profile));

    Scheduler scheduler;
    Dispatcher dispatcher(db);

    auto context = dispatcher.nextExecution(
        scheduler,
        "/tmp/cuBitCrack",
        0
    );

    assert(context);

    assert(context->jobId == jobId);
    assert(context->puzzleId == storedPuzzle->id);
    assert(context->rangeId == rangeId);
    assert(context->engine == "BitCrack");
    assert(!context->workspace.empty());
    assert(!context->command.empty());

    assert(contains(context->command, "/tmp/cuBitCrack"));
    assert(contains(context->command, "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"));
    assert(contains(context->command, "--keyspace"));
    assert(contains(context->command, "400000000000000000:40000000FFFFFFFFFF"));
    assert(contains(context->command, "-d 0"));
    assert(contains(context->command, "-b 256"));
    assert(contains(context->command, "-t 512"));
    assert(contains(context->command, "-p 2048"));
    assert(contains(context->command, "found.txt"));
    assert(contains(context->command, "bitcrack.log"));

    std::remove(dbPath);

    std::cout << "DispatcherExecutionTests passed\n";
    return 0;
}
