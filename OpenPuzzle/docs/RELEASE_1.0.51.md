# OpenPuzzle 1.0.51 — Vanity CUDA compatibility

The Vanity engine in 1.0.50 only contained sm_89 device code. On RTX 3080,
automatic tuning failed before requesting any assignment. This release replaces
that binary with native CUDA code for sm_50, 52, 60, 61, 70, 75, 80, 86, 89 and
90, plus compute_50 PTX for forward JIT compatibility with suitable drivers.

The corresponding source and build recipe are included. Legacy CPU hashing is
compiled with -fno-strict-aliasing to preserve correct Base58Check address and
WIF checksums under optimization. GPU kernels and search algorithms are unchanged.

Real known-address acceptance passed on one RTX 4070 SUPER and all five RTX 3080
cards, tested individually. Each checked exactly 65,536 keys and verified the
expected address and compressed WIF. Compilation targets for older NVIDIA
families are included; those families have not all been tested on hardware.
Concurrent production rig operation is separate from these short individual
tests. CUDA does not add an AMD/Intel Vanity backend.

The existing automatic benchmark per GPU, measured profiles, one-hour assignment
target, multi-GPU supervisor, UUID binding, status, telemetry, Stop cancellation,
Safe Stop and private wallet export are preserved. The website/API is unchanged.
Windows remains on its separate test release.

This preparation rechecks existing GPU evidence without rerunning GPU work.
It runs CPU reference vectors, three affected synthetic/UI suites and verifies
that DEB and TGZ contain the exact tested engine. Earlier test evidence is
preserved and is not described as a newly run full suite.

Finish active work with Safe Stop and close the application before installing.
Verify SHA256SUMS, then install the regular DEB. The portable updater DEB is
byte-identical; TGZ and matching GPL source are included. Profiles, checkpoints,
GPU identifiers and private test solution files are not release assets.
