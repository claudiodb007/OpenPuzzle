# OpenPuzzle 1.0.15

OpenPuzzle 1.0.15 adds safe concurrent CUDA and OpenCL GPU execution. A single
`openpuzzle run` command can now start one CUDA worker and one OpenCL worker on
separate physical GPUs.

## One command, two GPU backends

Concurrent execution is enabled explicitly, for example:

```bash
openpuzzle run --backend cuda --device 0 --with-opencl --opencl-device 1 --rusticl-enable radeonsi
```

CUDA and OpenCL use independent runtime slots, assignments, workspaces, engine
processes, progress uploads and completion reporting. Either slot can finish
and request its next range without taking control of the other slot.

## Validation before server contact

OpenPuzzle resolves and validates both GPU selections locally before creating
the concurrent workers or requesting an assignment. An unavailable device
index is rejected with the actionable `OP-GPU-001` diagnostic.

When Rusticl is requested, its driver selection is applied before OpenCL
enumeration so the device indexes shown to OpenPuzzle match the intended
OpenCL driver.

## Separate physical GPUs required

The CUDA and OpenCL selections must represent different physical GPUs. If both
backends resolve to the same PCI device, OpenPuzzle rejects the launch before
server contact. This prevents two workers from accidentally competing on one
GPU because different APIs assigned it different logical indexes.

## Fair scheduling remains unchanged

Concurrent execution does not change assignment priority. Every client keeps
the same right to receive work. Device performance is used only to size each
range for its target duration; it does not affect queue position, reputation
or assignment access.

## Compatibility

Existing one-backend commands and stored profiles continue to work. Concurrent
OpenCL execution is opt-in through `--with-opencl`; systems with only one GPU
can continue using the normal `openpuzzle run` workflow.

## Validation

The release source passed all 88 automated tests, including focused coverage
for invalid CUDA and OpenCL indexes, duplicate physical GPU selection, Rusticl
ordering and successful independent preflight. Live negative-path validation
confirmed that rejected launches request no assignment and leave existing CUDA
and OpenCL workers undisturbed.
