# OpenPuzzle

## Current release

**OpenPuzzle 1.0.2** provides a continuous Linux client for requesting,
executing and synchronizing OpenPuzzle assignments.

The public repository contains the client only. The coordination service
and website infrastructure are not included.

See [OpenPuzzle 1.0.2 release notes](docs/RELEASE_1.0.2.md) and the
[Client Runtime guide](docs/CLIENT_RUNTIME.md).


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

### OpenPuzzle 1.0.2

Implemented:

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
openpuzzle stop
```

CPU execution uses KeyHunt range mode and requires an explicit thread
count. `--with-cpu` starts independent GPU and CPU assignment slots.
`openpuzzle status` reports each slot separately.

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
sudo apt install ./OpenPuzzle-1.0.2-portable-16f5277f.deb
```

Start OpenPuzzle:

```bash
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
