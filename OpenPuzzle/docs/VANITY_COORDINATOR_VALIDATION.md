# Vanity native coordinator — validated local contract

Base commit: `25366231c427a487ae39f4fa08cb7d4924e10176`. Preview: 0.11.0.

The native HTTP client completed one synthetic local assignment on the measured
GPU, searching 68,719,476,736 keys in two contiguous segments of 34,359,738,368 keys.
The assignment ID was preserved across both launches; there was one claim and
one completion. Native checkpoints, exact per-batch engine coverage, cumulative
HTTP progress and the final terminal state were cross-checked before this commit.

Validation reused, without rebuilding or rerunning searches:

- Linux JUnit: 189/189 suites, no failed, errored or skipped suites.
- Isolated platform JUnit: 24/24 suites, no failed, errored or skipped suites.
- Real GPU report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-coordinator/coordinator-714y5026/real-runs/run-66p0d2ye`.
- Preparation report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-coordinator/coordinator-714y5026`.
- Native CLI SHA-256: `0bd46102006339f3b362779a4cf9fe79ea0d969f3cad161eb23b4c882701d271`.
- Real GPU summary SHA-256: `30fcf1d7ac2cc76cc287d28d95b5693cd3fb31c88d28e1cfd0aa2f05b2348c3c`.
- HTTP journal SHA-256: `07e81bad7263a4d2660eb88c7ae004784c24d6e0cc3d0193a07bc4509969c598`.
- Native session binding SHA-256: `ef2ee024f9c89f1e58337e4e10e17c6398c489e0abc2a26728f1c9e47593c162`.

The external GPU test wrapper was corrected to pass the measured physical GPU
UUID instead of the fixture default. The native UUID equality guard was retained;
native sources and binaries were unchanged by that wrapper correction. The
corrected kit, original wrapper backup and failed test report remain preserved.

This validates a loopback coordinator contract. Production Vanity assignments
remain disabled. No benchmark, installation, GPU setting changes, release or
publication was performed. Main-window controls and local Resume were already
validated in the base commit and are preserved here.

Next: integrate this validated contract with the ordinary RunSession and main
controls, including production pacing and recovery, before production enablement.
