# OpenPuzzle 1.0.7

OpenPuzzle 1.0.7 adds explicit OpenCL device and Rusticl profile control.
It is a client-only release and does not include private server code.

## Explicit OpenCL device

Use `--device` to select the OpenCL device used by BitCrack:

```bash
openpuzzle run --backend opencl --device 1
```

The selected device is preserved through GPU discovery, benchmarking and
the real runtime command.

## Rusticl driver profile

Use `--rusticl-enable` when Mesa Rusticl needs an explicit Gallium driver:

```bash
openpuzzle run \
    --backend opencl \
    --device 1 \
    --rusticl-enable radeonsi
```

OpenPuzzle validates this option before OpenCL discovery and exports the
corresponding `RUSTICL_ENABLE` value to the OpenCL execution environment.
The option is rejected for non-OpenCL backends.

## Concurrent GPU and CPU execution

When `--with-cpu` is used, OpenCL and Rusticl arguments remain limited to
the GPU slot. The CPU child does not inherit GPU-only device or profile
arguments.

## Validation

The RX 5500 XT was selected explicitly through Rusticl `radeonsi` and
validated with the bundled OpenCL BitCrack engine. Local functional tests
use Bitcoin Puzzle 20 and do not request central assignments.

Existing CUDA, OpenCL and CPU execution remains compatible. Installation
does not silently enable or start a service, and this release does not stop
or replace an active OpenPuzzle runtime.
