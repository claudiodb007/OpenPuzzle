# OpenPuzzle 1.0.52 — Vanity benchmark launch

The native Vanity benchmark requires its owner PID to match its actual parent.
The previous command ran through /bin/sh without an explicit exec. Shells that
fork the final command inserted an extra process, causing native initialization
to refuse the owner and every candidate to report zero samples.

The benchmark command now explicitly execs the engine so the normal client
remains its direct parent. Owner validation and parent-death cleanup remain
enabled. Bounded native diagnostics, exit codes and protocol/sample failures
are now visible instead of being silently discarded.

The synthetic benchmark fixture now enforces the real parent/owner check and
verifies that native failures are reported without replacing a saved profile.
Three affected suites cover the benchmark CLI, process timeout cleanup and
the common desktop route. Previous evidence is retained, not presented as a
new full-suite or GPU run.

The multi-architecture CUDA engine is byte-identical to 1.0.51: native sm_50,
52, 60, 61, 70, 75, 80, 86, 89 and 90 plus compute_50 PTX. The six existing
known-address results from one RTX 4070 SUPER and five RTX 3080 cards remain
valid for that engine. This preparation does not rerun GPU work or benchmark.
A normal real benchmark on the rig remains the next acceptance step.

Existing measured profiles, five Vanity geometry candidates, ordinary one-hour
range sizing, multi-GPU slots, Stop, Safe Stop, telemetry, private key export,
website API and scheduling are preserved. BitCrack, KeyHunt, Kangaroo and the
Vanity adapter are unchanged. Windows remains a separate test release.

Verify SHA256SUMS and finish active work before installing. DEB, byte-identical
portable updater DEB, TGZ and matching source are included. Private profiles,
GPU identifiers, checkpoints and test solution files are not release assets.
