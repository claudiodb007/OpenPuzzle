# Changelog

## 1.0.28 — GPU telemetry and thermal safety

- Add read-only NVIDIA and AMD GPU temperature, power draw and power-limit
  telemetry to `openpuzzle doctor`.
- Add a disabled-by-default local thermal policy with configurable warning and
  critical thresholds.
- Add `openpuzzle thermal` commands for querying, enabling, configuring and
  disabling the policy without changing GPU clocks, fans or power limits.
- Monitor active GPU executions every 30 seconds with transition warnings,
  five-minute reminders and recovery reporting.
- Add opt-in `--stop-on-critical` protection using the existing orderly stop,
  progress synchronization and assignment cancellation lifecycle.
- Block new assignments when a selected GPU is already critical and thermal
  protection is enabled.
- Scope CUDA observation to selected devices and resolve OpenCL physical
  sensors conservatively on mixed-vendor systems.
- Add a two-degree recovery margin and retain hot state through temporary
  sensor unavailability to prevent misleading message oscillation.
- Preserve existing execution behaviour and avoid all runtime telemetry calls
  while the policy remains disabled.
- Pass all 131 automated tests, including telemetry parsing, policy,
  configuration, runtime ownership, scope, output, startup and hysteresis
  contracts.

## 1.0.27 — Unlimited multi-CUDA supervisors

- Add `--devices all` to start one independent BitCrack supervisor for every
  detected CUDA GPU, without a fixed client-side GPU limit.
- Add comma-separated explicit selection such as `--devices 0,2,5` with
  duplicate, missing and invalid-device rejection before claiming work.
- Give every selected GPU its own `cuda-N` runtime slot, assignment,
  workspace, state file, PID file and lifecycle.
- Discover dynamic CUDA slots automatically in `status`, `stop` and
  `safestop`.
- Keep workers independent when one assignment finishes or one GPU fails, and
  stop the remaining workers if a possible solution is found.
- Prevent single-runtime and legacy concurrent modes from overlapping an
  active multi-CUDA execution.
- Retain exclusive single-device execution for Pollard Kangaroo and limit the
  new multi-CUDA mode to BitCrack linear puzzles.
- Preserve the existing server coordination protocol, client identity,
  release pipeline, desktop interface and updater contract.
- Pass all 121 automated tests, including CUDA selection, dynamic-slot and
  independent-worker argument contracts.

## 1.0.26 — Reproducible Linux release contract

- Generate the SHA-prefixed portable Debian package required by the updater
  directly from the normal release package.
- Generate identical versioned, stable updater and conventional SHA-256
  manifests as part of the release build.
- Add one command for the complete Linux configure, build, desktop UI, test,
  CPack, manifest and validation pipeline.
- Refuse release builds from a dirty Git worktree.
- Create the source archive directly from the exact release commit.
- Record branch, commit, commit date, tracked file count, automated test count
  and the CUDA, OpenCL and KeyHunt engine hashes in the release manifest.
- Validate all required release artefacts and their updater contract before
  publication.
- Exclude generated CTest runtime state from committed source archives.
- Retain the OpenPuzzle 1.0.25 client, engine and coordination protocol
  behavior while strengthening release production and verification.
- Pass all 117 automated tests, including the release identity contract.

## 1.0.20 — Puzzle-aware routing and resilient solution alerts

  * Added metadata-driven routing for linear and Pollard Kangaroo workloads.
  * Added verified metadata for puzzles 140, 145, 150, 155 and 160 while preserving Puzzle 71's linear path.
  * Added exact power-of-two Kangaroo claim and assignment validation.
  * Added an external, opt-in PSCKangaroo installer and CUDA execution adapter; the executable is not bundled.
  * Added Kangaroo command, progress and protected result-file contracts.
  * Added metadata-only possible-solution reporting that never transmits private-key material.
  * Added automatic 30-second retry after temporary solution-report network failures.
  * Preserved assignment fairness, Rusticl selection and process-identity protections.
  * Passed all 98 automated client tests plus the targeted retry test.


## 1.0.18 — Full Linux process identity

- Bind persisted Linux execution identity to boot ID, PID and process
  start time from `/proc/<pid>/stat`.
- Reject legacy or incomplete process identities as proof of a live
  execution.
- Persist `process.start_time` in execution workspaces.
- Harden execution sync, heartbeat, monitoring and recovery against
  same-boot PID reuse.
- Revalidate process identity throughout stop and escalation paths.
- Extend runtime markers with process start time.
- Use pidfd signalling for runtime SIGTERM delivery after identity
  validation.
- Harden update active-execution detection against same-boot PID reuse.
- Add regression coverage for legacy 1.0.17 markers, same-boot PID reuse
  and pidfd signalling.
- 89 automated tests passing.

## 1.0.17 — Boot-bound process identity

- Bind persisted client execution state to the current Linux `boot_id` in
  addition to the numeric PID.
- Treat state written by OpenPuzzle 1.0.16 or earlier without `boot_id` as
  untrusted for process-liveness decisions.
- Bind runtime-control markers to system boot identity so stale PID files
  cannot stop or block unrelated reused processes after a reboot.
- Bind background engine-supervisor workspaces to `process.boot_id` before
  they are trusted for monitoring or signalling.
- Validate boot identity in recovery, stop, update-safety and client-heartbeat
  paths before accepting a persisted PID as active.
- Preserve direct signalling only for child processes created by the current
  process and therefore not recovered from persistent state.
- Added regression coverage for reused live PIDs, mismatched boot identities
  and legacy PID-only state while retaining all 88 automated tests.

## 1.0.16 — Resilient execution recovery

- Detect executions interrupted by reboot, power loss or abrupt shutdown when
  the stored process is gone and `exit.code` was never written.
- Report interrupted assignments as `cancelled` with internal exit code `-3`.
- Preserve the latest available progress when reporting interrupted work.
- Keep local assignment state when synchronization temporarily fails.
- Remove recovered state only after accepted completion reporting or explicit
  assignment rejection.
- Recover CUDA and OpenCL runtime slots independently without cross-slot state
  removal.
- Added HTTP and dual-slot recovery regression coverage while retaining all 88
  automated tests.

## 1.0.15 — Safe concurrent CUDA and OpenCL GPUs

- Added concurrent CUDA and OpenCL execution from one `openpuzzle run` command.
- Added strict local validation of both GPU selections before any server assignment is requested.
- Reject invalid device indexes with actionable `OP-GPU-001` diagnostics.
- Reject CUDA and OpenCL selections that resolve to the same physical GPU.
- Apply the requested Rusticl driver selection before OpenCL device discovery.
- Keep independent runtime slots, assignments, workspaces and completion reporting for both GPUs.
- Preserve equal assignment access; hardware performance remains limited to range sizing.
- Added focused negative-path coverage and retained all 88 automated tests.

## 1.0.14 — Adaptive GPU calibration

- Added conservative post-assignment calibration for automatically managed CUDA and OpenCL profiles.
- Ignore warm-up readings, require sustained samples and use their median to resist transient spikes.
- Apply a 0.97 planning factor, blend 25% live data with 75% previous history and limit each adjustment to 15%.
- Preserve the selected blocks, threads and points; calibration changes only the speed used to size future work.
- Update only the exact GPU, backend and engine profile after confirmed successful completion.
- Skip CPU runs, manual GPU launches, failures, cancellations and legacy runtime states safely.
- Keep assignment access and scheduling priority equal for every client; performance is used only for range sizing.
- Added focused calibration, profile-update, state-storage and completion regression coverage.

## 1.0.13 — Actionable diagnostics

- Expanded `openpuzzle doctor` into a complete local readiness report.
- Added configuration, private local storage, bundled engine, hardware, backend, profile and runtime-state checks.
- Added a safe coordination-server probe using `HEAD` only; no assignment is requested.
- Added offline diagnostics through `openpuzzle doctor --offline`.
- Report selected and optional GPU profiles separately to avoid false warnings.
- Added stable codes `OP-DOCTOR-001` through `OP-DOCTOR-006` with recovery guidance.
- Extended CLI regression coverage for offline diagnostics and result fields.

## 1.0.12 — Safe automatic updates

- Added `openpuzzle update --safe` for updates while work is active.
- Finish the current range normally and block new assignments before installing.
- Wait for complete runtime shutdown to avoid confusing the old and new client.
- Verify the installed version and resume OpenPuzzle automatically.
- Attempt to resume work after installation or validation failures and provide
  actionable `OP-UPDATE-005`, `OP-UPDATE-006` and `OP-UPDATE-007` errors.
- Preserved the existing `--check`, `--download-only` and guarded default modes.

## 1.0.11 — Clear one-command onboarding

- Kept `openpuzzle run` as the only command required after installation.
- Added explicit first-use stages for hardware readiness, GPU profile creation,
  safe benchmarking and permission to contact the coordination server.
- Confirmed that assignments are not requested when automatic benchmarking
  fails and retained exact recovery commands with stable error codes.
- Added regression coverage for successful and failed onboarding output.
- Preserved automatic profile reuse without repeating onboarding on later runs.

## 1.0.10 — Simpler benchmark, recovery, errors and updates

- Expanded the automatic GPU benchmark matrix across more block and point
  values, with bounded CUDA-only 512-thread candidates.
- Preserved the VRAM safety cap and portable OpenCL thread matrix.
- Added an explicit private-key-found banner, protected wallet export and a
  visible key-found notice without private-key material.
- Added stable actionable error codes to first-use, benchmark, engine,
  configuration and doctor failures.
- Added `openpuzzle update`, `--check` and `--download-only`.
- Added HTTPS manifest retrieval, strict release filename parsing, SHA-256
  verification and Debian package metadata validation.
- Prevented installation while any assignment state or runtime marker is
  active; OpenPuzzle never stops active work automatically.
- Added regression and integration coverage for the new workflows.

## 1.0.2 — CPU assignment sizing and portability

- Changed automatic CPU assignments to target 60 minutes, matching CUDA
  and OpenCL.
- Preserved `--duration-minutes` as an explicit duration override.
- Added the selected backend to assignment requests.
- Added backend-aware CPU sizing for CPU-only clients using the primary
  execution slot.
- Used observed client speed only to size work, without changing
  scheduling priority or assignment fairness.
- Added compatibility with older clients that identify CPU execution
  through the dedicated CPU slot.
- Pinned the bundled KeyHunt executable to a portable
  `x86-64-baseline` build without AVX-512 instructions.
- Added an exact HTTP integration regression test for CPU backend
  assignment requests.
- Validated the release package with all 82 tests passing.
- Verified continuous three-thread CPU execution beyond the previous
  five-minute visibility window.

## 1.0.1 — Installation diagnostics

- Added the local `openpuzzle doctor` command.
- Added validation of the bundled CUDA, OpenCL and CPU engines.
- Added separate CUDA and OpenCL device detection.
- Added logical CPU count and usable-backend reporting.
- Added configuration-path diagnostics without requiring registration.
- Added help output and CLI regression coverage for `doctor`.
- Preserved the 1.0 runtime and engine protocol compatibility.

## 1.0.0 — Continuous client

- Added continuous assignment execution.
- Added automatic client registration, heartbeat and progress synchronization.
- Added assignment completion, failure and cancellation reporting.
- Added recovery of active and finished local executions.
- Added `run`, `status`, `stop`, `--once`, `--help` and `--version` workflows.
- Added protected local WIF export without displaying or uploading private keys.
- Added metadata-only solution notification.
- Added protected local runtime and assignment storage.
- Added BitCrack execution monitoring and progress parsing.
- Added Release-build test coverage for the public client.
- Prepared a client-only source and binary distribution.
- Added a safe, repeatable and stability-aware GPU benchmark.
- Added automatic first-use benchmarking before any server work request.

## 0.10-dev — Foundation/Core

- Added persistent execution model.
- Added `executions` table.
- Added `statistics` table.
- Added richer `ranges` lifecycle.
- Added `list-ranges`.
- Added `complete-job` for manually completing job/range.
- Added `stats` command.

## 0.10.1-dev

- Fixed Boost.Multiprecision expression-template compile error in RangeAllocator.


## 0.11-dev — Execution Engine foundation

- Added `execution_progress` table.
- Added `audit_log` table.
- Added `dashboard` command.
- Added simulated progress checkpoint support for dry-run tests.
- Added audit events for dry-runs and executions.
- Added documentation for the Execution Engine.

## 0.11.1-dev

- Fixed command dispatcher: `dashboard` and `audit` are now registered.
- Updated CLI banner to 0.11.1-dev.

## 0.11.2-dev

- Fixed linker error by adding concrete dashboard/audit implementations.


## 0.13-dev — BitCrack Output Parser

- Added `BitCrackOutputParser`.
- Added `parse-bitcrack-line` command.
- Parser detects speed, start key, end key, count step, found, error and finished lines.
- Added `docs/BITCRACK_PARSER.md`.


## 0.15-dev — ExecutionManager

- Added `ExecutionManager`.
- Added `execution-test` command.
- Added `docs/EXECUTION_MANAGER.md`.
- ExecutionManager runs commands through ProcessRunner and parses BitCrack-style output.


## 0.16-dev — ExecutionSession

- Added `ExecutionSession` model.
- Added `session-test` command.
- Added `docs/EXECUTION_SESSION.md`.
- Prepared the execution lifecycle model for recovery and replay.


## 0.17-dev — EventBus

- Added `EventBus`.
- Added basic execution-related events.
- Added `event-test` command.
- Added `docs/EVENT_BUS.md`.
