# Vanity local assignment runtime

Base: `2658bce08b2ffa7fc289b6d5eb64e837a2d03d04`. Development kit: 0.13.0.

`measured/LocalCoordinator.py` owns the single-assignment loopback HTTP contract
and explicit native RunSession command. `Coordinator.py` imports it directly.
Starting, stopping, listing and resuming local runs no longer load
`tests/CoordinatorSessionTests.py` or rewrite `sys.argv` to import a test module.

The server binds only `127.0.0.1`, validates the existing client and assignment,
allows one fresh claim and no claim on resume, and confirms completion only
after cumulative progress reaches the assigned count. All executable paths,
physical GPU UUID, measured planning rate and the native argument order remain
explicit. Contract validation remains active with Python optimization enabled.

Fault responses, lease expiry, transient failures, cancellation triggers and
synthetic engine defaults remain in the test subclass. Native coordinator tests
exercise that subclass around the same runtime contract used by the desktop.
The runtime itself contains no fault-mode selector or synthetic engine default.

New local metadata uses `openpuzzle_coordinator_desktop_v2`: the contract hash
pins `LocalCoordinator.py` instead of the test file. No previous run or seal is
migrated or rewritten. The preparation uses a separate Git worktree and profile
store; the 0.12.0 source, binaries, profiles, real GPU evidence and commit receipt
remain intact. Their original launcher remains associated with their own store.

Validation for this change is restricted to four suites:

- VanityLocalCoordinatorTests: real loopback requests, explicit command binding,
  refusal of duplicate claims, wrong identities, invalid progress and premature
  completion, original-port reuse, and import without process-state changes.
- VanityCoordinatorSessionTests: the 18 existing native HTTP/adapter scenarios,
  with test-only faults around the extracted runtime contract.
- VanityCoordinatorDesktopTests: the 12 prior stop/resume/review scenarios plus
  execution from a standalone measured directory with no test tree. That case
  stops and resumes the same assignment and refuses a changed runtime hash
  before launching another kernel.
- VanityRunSessionRouteTests: existing strict selection and production gate.

The user preparation compiles only the CLI, its required adapter, and route-test
target to bind the new worktree's helper paths. It does not rebuild the UI or
isolated platform, repeat the previous 192-suite validation or launch a real GPU
search. The new four-suite JUnit is separate; the previous 192/192 and 24/24
results are referenced with their hashes, without claiming a new full-suite run.

This remains a local synthetic Puzzle-20 preview. Production Vanity is disabled.
No change is made to the ordinary runtime, native C++ code, UI layout, engines,
power or fan settings. No benchmark, installation, commit or publication is
performed by the preparation. Future work: coordinator pacing and explicit
network/lease recovery, followed by production configuration and release gates.
