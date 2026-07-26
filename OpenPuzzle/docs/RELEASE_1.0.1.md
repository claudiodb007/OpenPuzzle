# OpenPuzzle 1.0.1

OpenPuzzle 1.0.1 is a diagnostics update for the public Linux client.
It preserves the 1.0 runtime behavior and engine protocol compatibility.

## Highlights

- New local `openpuzzle doctor` command.
- Validation of the bundled CUDA, OpenCL and CPU engines.
- Separate detection of CUDA and OpenCL devices.
- Logical processor count and usable-backend summary.
- Configuration presence and path reporting.
- CLI regression coverage for the diagnostic command.

## Safe local diagnostics

Run:

```bash
openpuzzle doctor
```

The command checks the local installation only. It does not run a
benchmark or engine search, register a client, send a heartbeat, request
an assignment, contact the coordination server or alter active work.

## Installation

The public package supports Ubuntu 24.04 or newer on x86-64:

```bash
sudo apt install ./OpenPuzzle-1.0.1-Linux-x86_64.deb
```

The package continues to include:

- `cuBitCrack` for NVIDIA CUDA;
- `clBitCrack` for compatible OpenCL GPUs;
- `keyhunt` in range mode for CPU execution.

## Compatibility

This release changes the OpenPuzzle client version to 1.0.1. The bundled
BitCrack engine identity remains 1.0.0 with protocol 2 because the engine
protocol did not change.

The website and coordination service require no update for this release.
