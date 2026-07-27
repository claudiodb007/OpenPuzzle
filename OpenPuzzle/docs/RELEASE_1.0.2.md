# OpenPuzzle 1.0.2

OpenPuzzle 1.0.2 improves CPU assignment sizing and portability for the
public continuous Linux client.

## Highlights

- Automatic CPU assignments now target 60 minutes, matching CUDA and
  OpenCL.
- Explicit `--duration-minutes` values continue to override the
  automatic duration.
- Assignment requests identify the selected CPU, CUDA or OpenCL
  backend explicitly.
- CPU-only clients using the primary local execution slot receive
  CPU-sized work based on their observed speed.
- Performance is used only to size work. It does not change scheduling
  priority, participant fairness or access to assignments.
- The bundled KeyHunt CPU engine uses the portable
  `x86-64-baseline` instruction set and contains no AVX-512
  instructions.
- Continuous CPU runtime and three-thread website visibility were
  verified beyond the previous five-minute window.

## Sixty-minute assignment sizing

Without an explicit duration, all supported backends request work for a
target duration of 60 minutes:

```bash
openpuzzle run
openpuzzle run --backend cpu --threads 3
```

An explicit duration remains available:

```bash
openpuzzle run --backend cpu --threads 3 --duration-minutes 30
```

The coordination service uses the client's observed speed only to
calculate the range size required for the requested duration. Faster
machines receive proportionally larger ranges, but do not receive
priority over other participants.

## CPU backend protocol

Version 1.0.2 sends the selected backend with every assignment request.
This distinguishes a CPU-only client using the `primary` execution slot
from a GPU client using the same slot.

The coordination service remains compatible with older concurrent
clients that identify CPU execution through the dedicated `cpu` slot.

## Installation

The verified Ubuntu 24.04 x86-64 package is:

```text
OpenPuzzle-1.0.2-portable-16f5277f.deb
```

Install or upgrade with:

```bash
sudo apt install ./OpenPuzzle-1.0.2-portable-16f5277f.deb
```

SHA256:

```text
16f5277f9d8360404ab36b323f171edf7fc92ea0791883173ed84855b9c3940f
```

The package includes:

- `cuBitCrack` for NVIDIA CUDA;
- `clBitCrack` for compatible OpenCL GPUs;
- portable `keyhunt` in range mode for CPU execution.

## Verification

The final package was validated with:

- 82 of 82 automated tests passing;
- an exact HTTP regression test for the backend claim payload;
- package metadata verification for version 1.0.2 and amd64;
- checksum verification of all three bundled engines;
- KeyHunt instruction-set verification;
- a clean upgrade from OpenPuzzle 1.0.1;
- a live CPU assignment containing 16,976,160,000 keys;
- stable three-thread CPU execution after more than nine minutes.

## Public release scope

This repository and its release packages contain the OpenPuzzle client.
The website, coordination service and central database implementation
are not included in the public distribution.
