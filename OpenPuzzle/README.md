# OpenPuzzle

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
./build/OpenPuzzle benchmark --real --auto --gpu 0
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

-   docs/
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
