# OpenPuzzle 1.0.53 — Vanity CPU wait

Five Vanity processes could occupy both CPU cores on a low-power rig while
waiting for CUDA. Each native process now requests CUDA blocking synchronization,
preserving the other context flags and verifying the selected policy. CUDA API
errors still fail initialization. Search kernels, geometry, match validation,
private output and process ownership checks are unchanged.

The exact candidate binary passed known-address, exact-coverage and compressed-WIF
checks on all five RTX 3080 cards. A separate 30-second simultaneous local run
reported 11,380.62 MKey/s in total and 1.77% host CPU across the native processes.
Those short-run figures describe this rig and are not a universal performance
guarantee. Previous production readings used different measurement windows.
The new binary has not been hardware-tested on every compiled GPU family.

The release preparer checks the existing logs and binary SHA-256 without repeating
GPU tests or tuning. It rebuilds the ordinary client version/header, runs four
affected CPU/synthetic/UI suites and verifies the packaged engine matches the
tested candidate. Three earlier suites and prior hardware evidence are retained
as earlier results, not presented as new full-suite coverage.

Native CUBIN targets remain sm_50,52,60,61,70,75,80,86,89,90, with compute_50 PTX.
Corresponding GPL source, manifests and the CUDA 12.x build recipe are included.
The existing automatic benchmark, saved profiles, one-hour assignment sizing,
multi-GPU supervisor, Stop, Safe Stop, telemetry, wallet export and server API
behavior remain as in 1.0.52. This release does not claim a newly verified
production website solution notification. Other engines and Windows are unchanged.

Verify SHA256SUMS and finish active work before installation. DEB, byte-identical
portable updater DEB, TGZ and matching source are included. No private profiles,
GPU UUIDs, validation logs or wallet files are published as assets.
