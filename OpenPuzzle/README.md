# OpenPuzzle

## Current release

**OpenPuzzle 1.0.18** hardens persistent Linux process identity against
reboots, power loss and Linux PID reuse.

Execution state, runtime-control markers and background engine supervisors
are now bound to the current Linux `boot_id`. A numerically reused PID from
an earlier boot is never trusted as proof that an OpenPuzzle execution is
still active.

The public repository contains the client only. The coordination service
and website infrastructure are not included.

See [OpenPuzzle 1.0.18 release notes](docs/RELEASE_1.0.18.md) and the
[client runtime guide](docs/CLIENT_RUNTIME.md).

## Open-source orchestration platform for cryptographic keyspace exploration

> **OpenPuzzle** is an orchestration platform for cryptographic keyspace
> exploration. It coordinates search engines such as **BitCrack**, while
> remaining engine-independent.

------------------------------------------------------------------------

## Overview

OpenPuzzle does **not** perform cryptographic searches itself.

Instead, it manages:

-   Puzzle metadata
-   Wallet and Hash160 databases
-   Range allocation
-   Job scheduling
-   Worker coordination
-   Benchmarking
-   Recovery
-   Engine discovery
-   Execution monitoring
-   Distributed execution (roadmap)

The current implementation focuses on the Bitcoin Puzzle project but is
designed to support multiple search engines.

------------------------------------------------------------------------

## Design Philosophy

> **OpenPuzzle coordinates work. Search engines execute work.**

The core never depends directly on a specific engine.

------------------------------------------------------------------------

## Current Features

-   SQLite persistence
-   Puzzle / Wallet / Hash160 databases
-   Range allocator
-   Scheduler
-   Dispatcher foundation
-   Worker management
-   Heartbeat service
-   GPU profile database
-   Benchmark framework
-   Recovery framework
-   Execution tracking
-   Execution workspaces
-   Engine framework
-   Engine registry
-   Engine discovery
-   Engine monitor
-   BitCrack integration
-   Dashboard service
-   Doctor command
-   Extensive automated tests

------------------------------------------------------------------------

## Architecture

``` text
                 OpenPuzzle
                      │
                Application
                      │
        ┌─────────────┴─────────────┐
        │                           │
     Commands                    Services
        │                           │
        └─────────────┬─────────────┘
                      │
                  Scheduler
                      │
                  Dispatcher
                      │
                EngineManager
                      │
        ┌─────────────┴─────────────┐
        │                           │
   EngineRegistry              EngineFactory
        │                           │
        └─────────────┬─────────────┘
                      │
                ISearchEngine
                      │
               BitCrackEngine
                      │
                EngineMonitor
                      │
              ExecutionManager
                      │
                ProcessRunner
```

------------------------------------------------------------------------

## Build

``` bash
./scripts/build.sh
```

or

``` bash
cmake -S . -B build
cmake --build build
```

------------------------------------------------------------------------

## Tests

``` bash
./scripts/test.sh
```

or

``` bash
cd build
ctest --output-on-failure
```

------------------------------------------------------------------------

## Common Commands

### Synchronize data

``` bash
./build/OpenPuzzle sync-data --dir data
```

### List puzzles

``` bash
./build/OpenPuzzle puzzle list
```

### Show puzzle

``` bash
./build/OpenPuzzle puzzle show 71
```

### Queue jobs

``` bash
./build/OpenPuzzle queue add --puzzle 71 --block-bits 40
./build/OpenPuzzle queue list
```

### Engine management

``` bash
./build/OpenPuzzle engine list
./build/OpenPuzzle engine info bitcrack
```

### Execute a job

``` bash
./build/OpenPuzzle start-job \
    --puzzle 71 \
    --job 1 \
    --engine bitcrack \
    --dry-run
```

### Benchmark

``` bash
./build/openpuzzle benchmark --real --auto --gpu 0
```

### Diagnostics

``` bash
./build/OpenPuzzle doctor
```

------------------------------------------------------------------------

## Current Release

### OpenPuzzle 1.0.18

Version 1.0.18 extends Linux process identity from `boot_id + PID` to
`boot_id + PID + process_start_time`, preventing a PID reused during the
same boot from being mistaken for an existing OpenPuzzle execution.

Execution state, workspace process markers, runtime control, heartbeat,
monitoring, stopping and update safety now validate the process start
time. Runtime SIGTERM delivery uses Linux pidfds after identity
validation.

See [the 1.0.18 release notes](docs/RELEASE_1.0.18.md).

### OpenPuzzle 1.0.17

Version 1.0.17 binds persistent execution identity to the Linux system
`boot_id` as well as the PID. Reused PIDs from a previous boot are rejected
by execution recovery, runtime control, engine-supervisor monitoring, update
safety checks and client heartbeat reporting.

Legacy state written by OpenPuzzle 1.0.16 or earlier does not contain a
`boot_id` and is therefore handled conservatively rather than trusted by PID
alone.

See [the 1.0.17 release notes](docs/RELEASE_1.0.17.md).

### OpenPuzzle 1.0.16

Version 1.0.16 adds resilient recovery for executions interrupted before the
background supervisor can write `exit.code`. Interrupted assignments are
reported safely as cancelled, temporary synchronization failures preserve
local state, and concurrent CUDA/OpenCL slots recover independently.

See [the 1.0.16 release notes](docs/RELEASE_1.0.16.md).

### OpenPuzzle 1.0.15

New in this release:

-   Concurrent CUDA and OpenCL execution using separate physical GPUs
-   Local validation of both devices before contacting the assignment server
-   Actionable errors for invalid indexes and duplicate physical GPU selection
-   Rusticl driver selection before OpenCL discovery
-   Independent assignments, workspaces and completion for both GPU slots
-   Equal assignment access preserved for every client

### OpenPuzzle 1.0.14

New in this release:

-   Adaptive speed calibration after confirmed successful GPU assignments
-   Median sustained-speed estimation after two warm-up readings
-   Conservative 0.97 planning factor, 25% blending and 15% adjustment limit
-   Exact GPU/backend/engine profile updates with launch settings preserved
-   Safe skips for CPU, manual, failed, cancelled and legacy executions
-   Equal scheduling rights preserved; speed affects only future range size

### OpenPuzzle 1.0.13

New in this release:

-   Complete readiness diagnostics through `openpuzzle doctor`
-   Local configuration, storage, engine, hardware and runtime-state checks
-   Selected-backend profile policy without false warnings for optional profiles
-   Safe coordination-server reachability probe using `HEAD` only
-   Offline diagnostics through `openpuzzle doctor --offline`
-   Stable diagnostic codes with concrete recovery actions

The release retains:

-   Safe automatic updates through `openpuzzle update --safe`
-   Active ranges finish normally while new assignments remain blocked
-   Complete runtime shutdown confirmation before package installation
-   Installed-version verification and automatic work resumption

-   Automatic 60-minute CPU, CUDA and OpenCL assignments
-   Backend-aware CPU range sizing using observed speed
-   Equal assignment access independent of participant performance
-   Portable x86-64-baseline KeyHunt without AVX-512

-   Continuous anonymous assignment processing
-   Unified CUDA and OpenCL GPU package
-   Bundled KeyHunt CPU range backend
-   Explicit CPU thread selection
-   Concurrent GPU and CPU execution slots
-   Per-slot progress, speed and checkpoint status
-   Completion proof and full-range server validation
-   Graceful stop, recovery and lease synchronization
-   Protected local solution handling

------------------------------------------------------------------------

## Continuous client

The client can run autonomously:

```bash
openpuzzle run
openpuzzle run 71
openpuzzle run --backend cpu --threads 8
openpuzzle run --with-cpu --cpu-threads 8
openpuzzle status
openpuzzle safestop
openpuzzle stop
```

CPU execution uses KeyHunt range mode and requires an explicit thread
count. `--with-cpu` starts independent GPU and CPU assignment slots.
`openpuzzle status` reports each slot separately.

`openpuzzle safestop` lets every active assignment finish and then exits before requesting new work. With concurrent GPU and CPU slots, each slot drains independently. `openpuzzle stop` remains the immediate cancellation command.

It requests random non-overlapping assignments, uploads progress, renews
leases, recovers interrupted sessions and continues with new work.

Potential solutions remain strictly local. openpuzzle stops execution and
preserves the private `found.txt` workspace without reading, printing or
uploading its contents. It submits only the assignment UUID and anonymous
client UUID as a pending report for independent review. A report never marks a
puzzle as solved automatically.

See [Continuous Client Runtime](docs/CLIENT_RUNTIME.md) for lifecycle,
recovery, cancellation and security details.

------------------------------------------------------------------------

## Roadmap

After 1.0:

-   Windows client package
-   Additional audited engine adapters
-   Improved installation diagnostics
-   Reproducible public release automation

------------------------------------------------------------------------

## Documentation

See:

-   [Continuous Client Runtime](docs/CLIENT_RUNTIME.md)
-   docs/architecture/
-   ROADMAP.md

------------------------------------------------------------------------

## Requirements

Package installation:

-   Ubuntu 24.04 or newer on x86-64
-   NVIDIA CUDA or a compatible OpenCL runtime for GPU execution
-   CPU execution is available through bundled KeyHunt range mode

Source builds additionally require:

-   GCC with C++20 support
-   CMake 3.22+
-   SQLite3 and Boost development libraries

------------------------------------------------------------------------

## Contributing

Pull requests, issues and suggestions are welcome.

------------------------------------------------------------------------

## License

MIT License.

------------------------------------------------------------------------

## Simple installation

Install the Debian package:

```bash
sudo apt install ./OpenPuzzle-1.0.10-portable-XXXXXXXX.deb
```

Start OpenPuzzle:

```bash
openpuzzle run
```

From 1.0.10 onward, check for later releases with:

```bash
openpuzzle update --check
```

When an update is available, let active work finish before installation:

```bash
openpuzzle safestop
openpuzzle update
openpuzzle run
```

On first use, GPU execution automatically selects CUDA or OpenCL,
validates the bundled engine and creates a safe benchmark profile before
contacting the coordination server. Later runs reuse the saved profile.

CPU execution uses bundled KeyHunt range mode and does not require a
benchmark. The number of CPU threads must be selected explicitly:

```bash
openpuzzle run --backend cpu --threads 8
openpuzzle run --with-cpu --cpu-threads 8
```

The GPU benchmark can be repeated manually when required:

```bash
openpuzzle benchmark --real --auto
```

Advanced users may install from source instead:

```bash
cmake -S OpenPuzzle -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

After a source installation, use the same `run` command.
