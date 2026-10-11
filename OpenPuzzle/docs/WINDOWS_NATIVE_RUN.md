# Windows measured-profile run and explicit recovery

Base: `2df6989adc46a7d1ce494228b762752bb7da093d`. This stage reuses the
previously validated native engines and sustained benchmark result files.

## Run contract

`run --profile ABSOLUTE_RESULT_JSON --server URL [--once] [--dry-run]`
selects exactly the backend, device, threads, blocks, points and measured speed
in a passed sustained benchmark. Explicit different launch parameters are
refused before registration or a claim. The executable SHA-256 and its native
CPU/GPU mapping are checked again before claiming and before starting work.
CPU, CUDA and OpenCL are supported. Kangaroo remains unsupported.

Automatic puzzle selection sends puzzle 0 and linear search mode; the server
chooses the lowest eligible open puzzle. Explicit puzzle selection still
requires a local metadata catalog at `puzzles/NUMBER.json` beside the portable
client; this source export does not supply that catalog. Missing metadata is a
local refusal before any request. The returned assignment is validated by the
existing planner and must agree with the selected backend and inclusive range.

Preflight runs only the engine identity/inventory commands. It does not start a
search, write client identity/configuration, register, heartbeat or claim work.
Runtime search remains explicit; no command launches automatically on login.

BitCrack Windows launch explicitly requests compressed addresses. Windows
KeyHunt receives an exclusive upper bound derived from the inclusive assignment
end. A solution is distinct from complete-range proof.

## CPU solution preservation

The native KeyHunt result `KEYFOUNDKEYFOUND.txt` is recognized, parsed as the
four-line compressed address-mode record, checked against the assignment
address and scalar range, and exported privately as hexadecimal. The original
file remains untouched. GPU WIF exports retain the existing contract. Export
failure prevents the Windows runtime from announcing the solution as ready for
review. Notifications contain only assignment/client identity, never a key.

## Explicit recovery

`recover [--server URL] [--slot NAME]` does not restart an engine or claim new
work. It requires an inactive runtime, the existing stable client identity, a
matching claim receipt/state/workspace, a verified stopped engine generation,
and the same server recorded in the claim. Completion is synchronized with the
existing proof policy. Network failure retains the exact state and receipt;
acknowledgement or authoritative rejection is required before cleanup.

A recovered solution is privately exported and only its review notification is
sent. State and receipt remain preserved, blocking further claims. An uncertain
pre-request intent, missing state, mismatched identity, running process or
unreadable data is never treated as permission to clear or retry the claim.
An acknowledgement followed by a crash before receipt cleanup can require
manual review; the conservative receipt block prevents duplicate allocation.

## Native validation boundary

`validate-native-run.ps1 -PriorReport PRIOR_BINDING_REPORT` creates a fresh,
short build/report directory under `%USERPROFILE%\op`. It compiles and runs 28
native suites, including profile/solution parsing and recovery/network-failure
fixtures, then runs the actual previously built KeyHunt/CUDA/OpenCL executables
against a loopback server with public synthetic scalars 1 and 2. Each backend
must complete exactly one key and separately detect/export scalar 2 without
requesting another assignment. The solution recovery path is also exercised.
The old binaries, profiles, reports and installed applications are preserved.
No new benchmark, engine rebuild or production claim is requested.

Local Linux validation checks 15 isolated portable tests. Windows cross-build
checks compilation/registration; native MSVC, Windows execution and GPU results
must be established by the user's native Windows report. Full Linux regression
contains 167 tests and is checked by guarded `integrate-linux.sh` after native
validation. UI, telemetry, installer and production validation remain pending.
