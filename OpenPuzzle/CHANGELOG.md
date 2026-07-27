# Changelog

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
