# OpenPuzzle 1.0.0

OpenPuzzle 1.0.0 is the first stable release of the continuous Linux
client.

## Highlights

- Continuous anonymous assignment requests and execution.
- Automatic retry while no work is available.
- Unified package containing CUDA, OpenCL and CPU engines.
- Automatic CUDA or OpenCL GPU selection and validated GPU profiles.
- KeyHunt CPU range mode with explicit thread selection.
- Concurrent GPU and CPU execution using independent slots.
- Per-slot speed, keys checked, checkpoint and workspace status.
- Heartbeat, progress and lease synchronization.
- Completion proof with full-range coverage validation.
- Recovery after interrupted client sessions.
- Local `status` and graceful `stop` commands.
- Single-assignment execution with `--once`.
- Secure detection and preservation of locally found solutions.
- Private runtime files and assignment workspaces.

## Installation

The public package supports Ubuntu 24.04 or newer on x86-64:

```bash
sudo apt install ./OpenPuzzle-1.0.0-Linux-x86_64.deb
```

The package includes:

- `cuBitCrack` for NVIDIA CUDA;
- `clBitCrack` for compatible OpenCL GPUs;
- `keyhunt` in range mode for CPU execution.

## Basic commands

```bash
openpuzzle --version
openpuzzle --help
openpuzzle run
openpuzzle status
openpuzzle stop
```

CPU-only execution with eight threads:

```bash
openpuzzle run --backend cpu --threads 8
```

Concurrent GPU and CPU execution:

```bash
openpuzzle run --with-cpu --cpu-threads 8
```

Use `openpuzzle run --once` to process only one assignment.

## Privacy and solution handling

When a search engine reports a potential result, OpenPuzzle stops
continuous execution, validates the structured result locally and
creates a protected compressed-WIF export under
`~/OpenPuzzle-Solutions`.

The private key is never displayed or uploaded. Only assignment
metadata notifies the coordination service that a solution requires
independent review.

## Public release scope

This repository and its release packages contain the OpenPuzzle client.
The website and coordination service implementation are not part of the
public distribution.

## Source build requirements

- Linux
- CMake 3.22 or newer
- A C++20 compiler
- SQLite3 and Boost development libraries
- Compatible CUDA or OpenCL development tooling when rebuilding GPU
  engines

Build and test:

```bash
cmake -S OpenPuzzle -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```
