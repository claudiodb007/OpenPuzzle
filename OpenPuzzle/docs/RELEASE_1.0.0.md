# OpenPuzzle 1.0.0

OpenPuzzle 1.0.0 is the first stable release of the continuous
OpenPuzzle client.

## Highlights

- Continuous assignment requests and execution.
- Automatic retry while no work is available.
- Heartbeat and search progress synchronization.
- Completion, failure and cancellation reporting.
- Recovery after interrupted client sessions.
- Local `status` and graceful `stop` commands.
- Single-assignment execution with `--once`.
- Secure detection and preservation of locally found solutions.
- Private runtime files and assignment workspaces.

## Privacy and solution handling

When the search engine writes a non-empty `found.txt`, OpenPuzzle stops
the engine and preserves the workspace and client state.

OpenPuzzle does not read, display or upload the private key. Only
assignment metadata is used to notify the coordination service that a
solution requires review.

## Public release scope

This repository and its release packages contain the OpenPuzzle client.
The website and coordination service implementation are not part of the
public distribution.

## Requirements

- Linux
- CMake 3.22 or newer
- A C++20 compiler
- SQLite3 development libraries
- Boost
- BitCrack with a compatible CUDA or OpenCL backend

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Basic commands

```sh
OpenPuzzle --version
OpenPuzzle --help
OpenPuzzle run
OpenPuzzle status
OpenPuzzle stop
```

Use `OpenPuzzle run --once` to process only one assignment.
