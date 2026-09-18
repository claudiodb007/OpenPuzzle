# OpenPuzzle 1.0.27 — Unlimited multi-CUDA supervisors

OpenPuzzle 1.0.27 allows one client command to use every available CUDA GPU
for independent BitCrack work. It is designed for systems ranging from two
GPUs to larger mining rigs, without a fixed client-side GPU-count limit.

## Multi-CUDA execution

Use every detected CUDA GPU:

    openpuzzle run 71 \
      --engine bitcrack \
      --backend cuda \
      --devices all

Or select an explicit set of device indexes:

    openpuzzle run 71 \
      --engine bitcrack \
      --backend cuda \
      --devices 0,2,5

The selector validates the CUDA inventory before any assignment is requested.
Empty lists, duplicate indexes, negative values, missing devices and
incompatible execution options are rejected locally.

## Independent supervisors

Every selected GPU receives a dynamic `cuda-N` execution slot. Each slot has
its own:

- server assignment and non-overlapping range;
- BitCrack process and selected CUDA device;
- assignment workspace and result file;
- client state and runtime PID file;
- progress, completion, cancellation and recovery lifecycle.

There is no fixed client-side limit on the number of CUDA workers. The actual
limit is the number of usable CUDA devices reported by the host.

One completed or failed worker does not stop the other workers. If a possible
solution is found, the supervisor requests an orderly stop of every remaining
slot so the result can be preserved and reviewed safely.

## Unified runtime controls

Dynamic `cuda-N` slots are discovered automatically. Existing commands control
the complete rig without requiring the operator to enumerate devices:

    openpuzzle status
    openpuzzle safestop
    openpuzzle stop

`status` reports every active CUDA slot independently. `safestop` lets every
slot finish its current assignment and then exit. `stop` requests immediate
orderly cancellation for all active slots.

OpenPuzzle rejects a new single-runtime, CUDA/OpenCL concurrent or multi-CUDA
start when it would overlap an already active execution.

## Engine compatibility

Multi-CUDA mode currently supports BitCrack linear puzzles only. Pollard
Kangaroo remains an exclusive single-device execution because its checkpoint,
memory and walk coordination contract is different from independent linear
ranges.

KeyHunt CPU and the established CUDA plus OpenCL mode are unchanged. The
coordination API, fairness rules, client identity and assignment protocol are
also unchanged.

The Qt desktop interface retains its existing execution modes in this release.
Multi-CUDA rig operation is available through the command-line client.

## Validation

The complete suite contains 120 automated tests. New coverage validates:

- unlimited CUDA inventory resolution;
- explicit and automatic device selection;
- dynamic `cuda-N` slot identities and filesystem discovery;
- per-device worker command construction;
- invalid or conflicting selector rejection;
- preservation of existing runtime and release contracts.

## Upgrade and rig test

Allow active assignments to finish before replacing an installed client:

    openpuzzle safestop

Install the Debian package:

    sudo apt install ./OpenPuzzle-1.0.27-Linux-x86_64.deb

Confirm the installation and CUDA inventory:

    openpuzzle --version
    openpuzzle doctor

On a six-GPU rig, start the independent workers with:

    openpuzzle run 71 \
      --engine bitcrack \
      --backend cuda \
      --devices all

Then verify that `openpuzzle status` reports slots `cuda-0` through `cuda-5`.

The portable updater package keeps the established identity:

    OpenPuzzle-1.0.27-portable-XXXXXXXX.deb
