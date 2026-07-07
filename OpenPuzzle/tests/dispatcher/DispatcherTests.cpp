#include "openpuzzle/dispatcher/Dispatcher.hpp"
#include "openpuzzle/database/Database.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>

using namespace openpuzzle;

int main() {
    const char* dbPath = "/tmp/openpuzzle_dispatcher_test.db";
    std::remove(dbPath);

    Database db;
    assert(db.open(dbPath));
    assert(db.createSchema());

    PuzzleRecord puzzle;
    puzzle.number = 71;
    puzzle.name = "Puzzle 71";
    puzzle.address = "test-address";
    puzzle.hash160 = "test-hash160";
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
    worker.speedMkeys = 1300.0;
    worker.temperature = 55.0;
    worker.power = 210.0;

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

    Dispatcher dispatcher(db);
    auto task = dispatcher.next();

    assert(task);
    assert(task->valid);

    assert(task->jobId == jobId);
    assert(task->puzzleId == storedPuzzle->id);
    assert(task->rangeId == rangeId);

    assert(task->workerId == workerId);
    assert(task->engine == "BitCrack");
    assert(task->backend == "CUDA");

    assert(task->rangeStart == "400000000000000000");
    assert(task->rangeEnd == "40000000FFFFFFFFFF");

    assert(task->hasProfile);
    assert(task->profile.gpuName == "RTX 4070 Super");
    assert(task->profile.backend == "CUDA");
    assert(task->profile.engine == "BitCrack");
    assert(task->profile.blocks == 256);
    assert(task->profile.threads == 512);
    assert(task->profile.points == 2048);
    assert(task->profile.averageSpeed == 1350.0);

    std::remove(dbPath);

    std::cout << "DispatcherTests passed\n";
    return 0;
}
