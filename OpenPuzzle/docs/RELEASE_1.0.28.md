# OpenPuzzle 1.0.28 — GPU telemetry and thermal safety

OpenPuzzle 1.0.28 adds read-only GPU telemetry and an optional thermal safety
policy for single-GPU, concurrent and multi-GPU execution. Existing runtime
behaviour is retained unless the operator explicitly enables the policy.

## Read-only GPU telemetry

`openpuzzle doctor` now reports available temperature, power draw and power
limit readings for NVIDIA and AMD GPUs. NVIDIA data is read through
`nvidia-smi`; AMD data is read from the Linux `amdgpu` hwmon interface.

AMD cards can expose edge, junction and memory readings. OpenPuzzle reports
the highest valid sensor temperature as the conservative device value. Missing
or unsupported measurements are shown as unavailable rather than zero.

Telemetry never changes power limits, clocks or fan control.

## Thermal policy

The policy is disabled by default. Inspect or configure it locally with:

    openpuzzle thermal
    openpuzzle thermal --enable --warning-c 75 --critical-c 85
    openpuzzle thermal --enable --stop-on-critical
    openpuzzle thermal --diagnostic-only
    openpuzzle thermal --disable

Threshold validation rejects invalid and conflicting changes before rewriting
the configuration. Diagnostic mode observes and reports temperature without
signalling or stopping an engine.

## Runtime monitoring

When enabled, the active runtime samples GPU telemetry every 30 seconds. It
reports warning and critical transitions, repeats a persistent warning at most
once every five minutes, and announces recovery after the temperature falls
at least two degrees below the warning threshold.

A temporary missing sensor reading does not erase an existing hot state. This
prevents misleading warning and recovery oscillation near a threshold or
during a transient driver query failure.

Only the parent supervisor owns monitoring. Supervised workers do not perform
duplicate telemetry queries, including on six-GPU rigs.

## Optional critical protection

`--stop-on-critical` is explicit and disabled by default. At the first
critical reading, OpenPuzzle uses the normal orderly stop lifecycle:

- final progress is synchronized;
- the engine is stopped by its owning runtime;
- assignment cancellation is reported;
- local state is removed only through the established synchronization path.

For concurrent and multi-GPU execution, every worker is asked to stop so the
remaining devices do not continue producing heat. OpenPuzzle also checks the
selected thermal scope before requesting new work. If a selected GPU is
already critical, startup is blocked and no assignment is requested.

## Device scope

CUDA monitoring follows the selected physical devices. For example,
`--devices 1,2` observes only `cuda-1` and `cuda-2`; an unrelated hot CUDA GPU
cannot stop that execution.

OpenCL indexes do not always expose stable physical identifiers. OpenPuzzle
resolves a sole AMD or NVIDIA device and complete same-vendor selections. When
only an ambiguous subset of identical-vendor devices is selected, monitoring
falls back conservatively to the whole host instead of guessing.

CPU-only execution performs no GPU telemetry queries.

## Compatibility

- Existing OpenPuzzle configuration remains valid.
- The policy remains disabled until explicitly enabled.
- BitCrack CUDA and OpenCL, KeyHunt CPU and optional PSCKangaroo routing are
  unchanged.
- Dynamic `cuda-N` and `opencl-N` execution slots are unchanged.
- The coordination API, assignment fairness and client identity are unchanged.
- The desktop interface and updater contract are unchanged.

## Validation

The complete suite contains 131 automated tests. Coverage includes:

- NVIDIA and AMD telemetry parsing;
- conservative AMD sensor selection;
- thermal threshold and configuration validation;
- disabled-policy zero-overhead contracts;
- supervisor-only monitoring ownership;
- CUDA and unambiguous OpenCL physical-device scope;
- diagnostic and enforced action output;
- critical startup blocking before assignment requests;
- recovery hysteresis and temporary sensor unavailability.

## Upgrade

Allow active assignments to finish before replacing the installed package:

    openpuzzle safestop

Install the Debian package:

    sudo apt install ./OpenPuzzle-1.0.28-Linux-x86_64.deb

Confirm the installation and inspect telemetry:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle thermal

The portable updater package retains the established identity:

    OpenPuzzle-1.0.28-portable-XXXXXXXX.deb
