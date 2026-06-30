# OpenPuzzle

**Measure everything. Assume nothing.**

OpenPuzzle is a Linux-first research platform for managing Bitcoin Puzzle keyspace exploration.

It is not a solver. It coordinates ranges, jobs, executions, tools, GPU selection, statistics and future community sync.

## Current milestone: Foundation/Core

This development build introduces the permanent execution model:

```text
Puzzle -> Range -> Job -> Execution -> Statistics
```

## Ubuntu dependencies

```bash
sudo apt update
sudo apt install build-essential cmake libsqlite3-dev libboost-all-dev nvidia-utils
```

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Quick start

```bash
./OpenPuzzle init
./OpenPuzzle import-puzzle-json --file ../resources/puzzles/71.json
./OpenPuzzle configure-tool --bitcrack /home/claudiodb/BitCrack/bin/cuBitCrack
./OpenPuzzle gpu-list
./OpenPuzzle gpu-select --device 0
./OpenPuzzle create-job --puzzle 71 --block-bits 40
./OpenPuzzle list-ranges --puzzle 71
./OpenPuzzle start-job --puzzle 71 --job 1 --dry-run
```

Automatic community upload is intentionally disabled. For Puzzle 71, external ranges can be imported read-only, but local ranges are not shared automatically.


## v0.11-dev commands

```bash
./OpenPuzzle dashboard --puzzle 71
./OpenPuzzle start-job --puzzle 71 --job 1 --dry-run --simulate-progress
```

This build adds the first execution progress/audit foundation. Real-time BitCrack parsing is the next step.


## GitHub-ready

This package includes:

- MIT license
- `.gitignore`
- GitHub Actions build workflow
- smoke test script
- initial architecture docs

See:

```text
docs/GITHUB_SETUP.md
docs/TESTING.md
```


## BitCrack parser test

```bash
./OpenPuzzle parse-bitcrack-line --line "[Info] 1334.62 MKey/s"
./OpenPuzzle parse-bitcrack-line --line "[Info] Starting at: 0000000000000000000000000000000000000000000000400000000000000000"
```


## ExecutionManager test

```bash
./OpenPuzzle execution-test
```
