# OpenPuzzle

## Current release

**OpenPuzzle 1.0.0** provides a continuous Linux client for requesting,
executing and synchronizing OpenPuzzle assignments.

The public repository contains the client only. The coordination service
and website infrastructure are not included.

See [OpenPuzzle 1.0.0 release notes](docs/RELEASE_1.0.0.md) and the
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

### release/0.40

Implemented:

-   Dashboard Service
-   Dispatch Service
-   Graceful shutdown
-   Engine Framework
-   Engine Registry
-   Engine Discovery
-   Engine Manager
-   Engine Monitor
-   BitCrack Engine
-   Engine List
-   Engine Info
-   `start-job --engine`
-   BitCrack command generation tests

------------------------------------------------------------------------

## Continuous client

The client can run autonomously:

```bash
openpuzzle run
openpuzzle run 71
openpuzzle status
openpuzzle stop
```

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

### 0.50

-   KeyHunt engine
-   Multi-engine execution
-   Continuous workers
-   Improved dispatcher

### 0.60

-   Distributed workers
-   REST API
-   Web dashboard
-   Multi-node clusters

------------------------------------------------------------------------

## Documentation

See:

-   [Continuous Client Runtime](docs/CLIENT_RUNTIME.md)
-   docs/architecture/
-   ROADMAP.md

------------------------------------------------------------------------

## Requirements

-   Linux
-   GCC (C++20)
-   CMake 3.22+
-   SQLite3

------------------------------------------------------------------------

## Contributing

Pull requests, issues and suggestions are welcome.

------------------------------------------------------------------------

## License

MIT License.

------------------------------------------------------------------------

## Simple installation

After installing the Debian package, prepare the local client once:

```bash
openpuzzle install
```

OpenPuzzle automatically selects CUDA or OpenCL, validates the bundled
engine, and runs the safe GPU benchmark only when a valid profile is not
already available. This step does not request server work.

Start continuous work with:

```bash
openpuzzle run
```
