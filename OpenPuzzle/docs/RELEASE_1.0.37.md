# OpenPuzzle 1.0.37 — Preserve uncertain identity across runtime controls

OpenPuzzle 1.0.37 fixes runtime commands that could treat a temporarily unreadable
process identity as proof that work had stopped. It also includes dynamic GPU
worker slots in diagnostic and update checks and preserves safe-stop requests
when a peer request fails.

## Status and diagnostics

`status` reports `identity unavailable` when an identity read cannot be completed.
It applies this distinction to both supervisor markers and saved engine state,
including an engine that outlives its supervisor. Confirmed inactive identities
retain the established idle or stopped behavior.

`doctor` counts uncertain identities separately under `Unknown identities` and
reports `OP-DOCTOR-006`. An uncertain live process is no longer labelled as a stale
PID file or as a confirmed stopped assignment. These checks cover primary, legacy
slots and discovered `cuda-N` / `opencl-N` workers.

## Safe stop

`safestop` inspects primary and concurrent runtime slots together. An uncertain
identity or failed request returns an error without claiming that all new work
has been blocked. Requests already made for other slots remain in place.

Repeated requests do not truncate an existing safe-stop file. If identity becomes
unreadable after creating a request, the request is retained and failure is
reported so the user can inspect status and retry. Confirmed inactive controls
retain their established cleanup behavior.

## Updates

Regular installation and `update --safe` refuse uncertain supervisor or engine
identities. Update checks include dynamic GPU workers and are repeated before
installation. Read-only `update --check` and `--download-only` remain available.
Safe-update request failures preserve requests already made to other slots;
waiting continues while an identity remains active or temporarily unavailable.

## Validation

The correction passed all 140 native client tests on Ubuntu before this release
identity was applied. The release build reruns the full suite against the
versioned source before packaging.

RuntimeIdentityVisibilityTests keeps a real child alive through a pipe and
injects unreadable identities, boot reads and existence probes locally to that
test target. GPU discovery and telemetry are empty test fixtures. It verifies
command output and failure codes, state and request preservation, dynamic worker
inspection, mixed safe-stop outcomes and confirmed inactive identities.

The new regression sends no termination signal, starts no search, requests no
assignment and invokes no updater download or installation. Existing process
identity, native pidfd, recovery, acquisition and UI tests remain in the suite.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect any identity warning and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.37-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.37-portable-XXXXXXXX.deb
