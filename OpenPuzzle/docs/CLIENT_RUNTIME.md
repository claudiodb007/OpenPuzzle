# Continuous Client Runtime

The OpenPuzzle client can request, execute and synchronize assignments
continuously. Search engines perform the cryptographic work; OpenPuzzle
coordinates assignment lifecycle, progress, recovery and finalization.

## Commands

Start continuous execution using the lowest-numbered active unsolved puzzle:

```bash
openpuzzle run
```

On first use, `run` selects an available bundled GPU backend and creates a
validated local benchmark profile before any registration, heartbeat or
assignment request. Later runs reuse that profile. `--dry-run` never starts
the benchmark or contacts the server.

Request work for a specific puzzle:

```bash
openpuzzle run 71
```

Run one independent BitCrack supervisor for every detected CUDA device:

```bash
openpuzzle run 71 --engine bitcrack --backend cuda --devices all
```

An explicit comma-separated selection is also accepted, with no fixed client
limit on the number of devices:

```bash
openpuzzle run 71 --engine bitcrack --backend cuda --devices 0,2,5
```

Each selected GPU receives its own assignment, workspace, PID file and local
state in a `cuda-N` slot. `status`, `stop` and `safestop` discover all such
slots automatically. Multi-CUDA currently supports BitCrack linear puzzles;
Kangaroo remains an exclusive single-device execution. The supervisor performs
global device discovery once, passes each validated device to its worker and
staggers worker startup. Workers do not repeat the global BitCrack probe, which
avoids unnecessary GPU contexts and host-memory peaks. CUDA and OpenCL
inventories are cached before workers are forked; registration, heartbeat,
profile selection and status output reuse the inherited snapshot.
Legacy single-device runs retain their existing discovery and first-run setup
path; the supervised shortcut is added only to workers created by `--devices`.

Supervised CUDA workers are isolated with `CUDA_VISIBLE_DEVICES` before the
engine starts.  The public slot and saved runtime state continue to use the
physical device number (`cuda-0`, `cuda-1`, and so on), while cuBitCrack sees
only that selected GPU as logical device zero.  This prevents every worker
from creating CUDA contexts on every NVIDIA GPU in the host.  Worker launches
are separated by ten seconds so transient CUDA initialization allocations do
not overlap on memory-constrained rigs.  Normal `--device` execution is not
isolated and retains its previous behavior.

The same unlimited supervisor model is available for OpenCL devices:

```bash
openpuzzle run 71 --engine bitcrack --backend opencl --devices all
openpuzzle run 71 --engine bitcrack --backend opencl --devices 1,3,5
```

Each OpenCL worker uses an independent `opencl-N` slot. This includes AMD
devices exposed through Rusticl; `--rusticl-enable radeonsi` is preserved in
every worker command when supplied. Legacy single-device `cuda` and `opencl`
slots remain supported. OpenCL workers use the same single-probe and staggered
startup contract as CUDA workers.

Execute only one assignment:

```bash
openpuzzle run --once
```

Inspect local execution state:

```bash
openpuzzle status
```

Request shutdown after every active assignment finishes:

```bash
openpuzzle safestop
```

Stop the active runtime and cancel its current assignment immediately:

```bash
openpuzzle stop
```

Preview local configuration without registration, heartbeat, assignment or
lease creation:

```bash
openpuzzle run --dry-run
```

## Continuous lifecycle

The continuous runtime performs this cycle:

1. Validate local configuration and create a GPU profile when required.
2. Register the client and publish its capabilities.
3. Request a random non-overlapping assignment.
4. Start the configured search engine.
5. Upload public progress metrics and renew the assignment lease.
6. Upload completion, failure or cancellation.
7. Request another assignment.

When no work is available, the client remains in the `waiting` state and
retries without exiting.

Client performance may be used to size an assignment and select a compatible
engine or backend. It must never grant scheduling priority, reputation-based
access or preferential treatment.

## Read-only GPU telemetry

`openpuzzle doctor` collects a point-in-time hardware snapshot without
changing clocks, fans, power limits, assignments or runtime state. NVIDIA
temperature, power draw and configured power limit are read with
`nvidia-smi`. AMD temperature and power sensors are read directly from the
kernel `amdgpu` hwmon interface under `/sys/class/drm`.

AMD cards can expose edge, junction and memory temperature sensors. The
reported value is the highest valid sensor reading, which is the conservative
choice for later safety decisions. A missing tool or unsupported sensor is
shown as `unavailable`; it is never represented as a zero measurement.

This telemetry layer is observational only. With the thermal policy disabled,
normal single-GPU, multi-GPU, CUDA, OpenCL and CPU execution paths do not call
it and retain their existing behaviour.

The local configuration also contains a disabled-by-default thermal policy:

```json
"thermal": {
  "thermal_enabled": false,
  "thermal_stop_on_critical": false,
  "thermal_warning_c": 75,
  "thermal_critical_c": 85
}
```

When enabled, `openpuzzle doctor` classifies each reading as `NORMAL`,
`WARNING`, `CRITICAL` or `UNAVAILABLE`. Thresholds are accepted only when the
warning value is between 30 and 110 degrees Celsius and the critical value is
higher, up to 120 degrees Celsius. The default policy remains diagnostic: it
never signals, stops or restarts a runtime and never changes GPU power, clocks
or fans.

The policy is managed locally from the command line:

```bash
openpuzzle thermal
openpuzzle thermal --enable --warning-c 75 --critical-c 85
openpuzzle thermal --enable --stop-on-critical
openpuzzle thermal --diagnostic-only
openpuzzle thermal --warning-c 72 --critical-c 82
openpuzzle thermal --disable
```

Querying the policy does not create or rewrite the configuration. Invalid,
missing, repeated or conflicting options are rejected before any file is
changed. Enabling the policy activates diagnostic classifications in
`openpuzzle doctor` and read-only runtime observation. A running client samples
the available GPU sensors once every 30 seconds. It writes a warning when a
device crosses the warning or critical threshold, repeats a persistent warning
at most once every five minutes, and reports recovery after the temperature
returns to normal. Configuration changes take effect when the next runtime is
started.

Single-GPU execution owns its observer directly. Concurrent and multi-GPU
execution assigns observation to the parent supervisor; supervised workers are
explicitly excluded, so a six-GPU run still executes only one `nvidia-smi`
telemetry query per sample interval rather than one query per worker. NVIDIA
and AMD readings are collected in the same machine snapshot.

In the default diagnostic-only mode, runtime observation never signals, stops
or restarts an engine, cancels an assignment, or changes GPU power, clocks or
fans. Disabling the policy removes runtime telemetry calls completely.

Critical protection is a separate opt-in setting and is disabled by default.
With `--stop-on-critical`, the first `CRITICAL` runtime reading requests the
same orderly shutdown lifecycle used by `openpuzzle stop`: final progress is
synchronized, the engine is terminated by its owning runtime, and assignment
cancellation is reported before local state is removed. A multi-GPU supervisor
requests the stop of every worker so the rig does not continue generating heat
on the remaining devices. The protection never changes power limits, clocks or
fans and never performs an abrupt engine kill. Restore warning-only behaviour
with `openpuzzle thermal --diagnostic-only`. Thermal event output states
`diagnostic only; execution continues` when enforcement is disabled and
`orderly stop requested` when critical protection is active, so the displayed
action always matches the configured policy.

Critical protection also samples the selected thermal scope before requesting
new work. If a selected GPU is already at or above the critical threshold,
single-GPU, concurrent and multi-GPU startup is blocked before an assignment
is requested. Diagnostic-only mode still reports the reading and continues.
No startup telemetry is collected while the policy is disabled.

CUDA observation is scoped to the physical devices selected by the execution.
A single `--device 2` run observes only `cuda-2`, while a multi-GPU
`--devices 1,2` supervisor observes only `cuda-1` and `cuda-2`. A hot GPU that
is not part of that execution can still be reported by `openpuzzle doctor`,
but it cannot stop unrelated CUDA work. CPU-only execution performs no GPU
telemetry queries.

OpenCL device indexes do not expose the same physical identifiers as NVIDIA
telemetry or Linux DRM cards. OpenPuzzle resolves the scope only when the
mapping is unambiguous: a sole AMD or NVIDIA device is matched to that
vendor's telemetry, and selecting every device of one vendor safely includes
all of that vendor's sensors. This covers common mixed NVIDIA plus AMD hosts,
including a CUDA GPU beside one Rusticl Radeon GPU. If several same-vendor
devices exist and only a subset is selected, OpenPuzzle retains conservative
whole-host monitoring rather than guessing the physical mapping. Mixed CUDA
plus OpenCL mode combines both resolved scopes when possible.

## Local states

`openpuzzle status` can report:

- `idle`: no runtime and no local execution;
- `waiting`: the continuous runtime is waiting for work;
- `running`: an engine is processing an assignment;
- `stopped`: the engine stopped and synchronization is pending;
- `solution found`: a non-empty local `found.txt` was detected.

## Recovery

Assignment state is stored in:

```text
~/.local/share/OpenPuzzle/client.state
```

If OpenPuzzle terminates while the engine remains active, a later
`openpuzzle run` attaches to the same process and assignment.

If the engine already terminated, OpenPuzzle synchronizes its final state
before requesting new work. Temporary network failures retain local state and
are retried.

Assignments rejected permanently by the server are stopped and released
locally. Invalid local or protocol state is preserved for diagnosis.

## Graceful safestop

`openpuzzle safestop` writes a local request for each active runtime slot. The current assignment continues normally, including progress and completion synchronization. After finalization, the slot exits before claiming another assignment. In concurrent GPU and CPU mode, both slots finish independently. Repeating the command is safe.

`openpuzzle status` is a strictly local, read-only inspection. It may read the
slot state, process identity, engine log and exit code, but it never uploads
progress or completion, changes an assignment, calibrates a profile or removes
state. Only the supervisor that owns a slot performs synchronization. This
prevents frequent desktop-interface status polling from racing a completed
engine and removing its state before the supervisor observes completion.

CUDA and OpenCL concurrent slots inherit the same puzzle and engine.
Kangaroo execution is exclusive because Pollard Kangaroo is CUDA-only;
the client rejects `--with-cpu` and `--with-opencl` for Kangaroo
workloads before requesting an assignment.

The request does not terminate an engine and does not cancel searched coverage. If no runtime is active, the command reports that there is nothing to stop.

## Cancellation

`Ctrl+C`, `SIGTERM` and `openpuzzle stop` request an orderly shutdown:

1. Synchronize final public progress when possible.
2. Stop the complete engine process group.
3. Report cancellation with the final key counter.
4. Preserve searched coverage on the server.
5. Remove local active state only after a safe final response.

If the server already rejected or finalized the assignment, openpuzzle stops
the engine without repeatedly submitting the same transition.

An active runtime also protects its assignment state against a temporary
failure while reading the engine process identity. Without a launcher
`exit.code`, the client preserves `client.state` and retries monitoring instead
of reporting a false interruption, cancelling the assignment or leaving an
unmonitored engine process behind.

## Solution safety

Each assignment uses:

```text
~/.local/share/OpenPuzzle/assignments/<assignment-id>/
```

A non-empty `found.txt` is treated as a potential solution.

When detected, OpenPuzzle:

- stops continuous execution;
- stops the engine if it is still active;
- preserves `client.state`, `found.txt` and the entire workspace;
- validates the structured result locally and checks that its address matches
  the assignment;
- creates `~/OpenPuzzle-Solutions/Puzzle-N/<assignment-id>/wallet-import.txt`
  with a compressed WIF key;
- protects solution directories with mode `700` and the wallet file with mode
  `600`;
- displays only filesystem paths and never displays or uploads the private key;
- submits only `assignment_id` and the anonymous `client_id` for review;
- prevents `openpuzzle stop` from deleting the preserved solution.

The wallet-import file is designed for local import into a trusted wallet. The
operator should first make a secure offline backup and verify the address.
Never paste a private key into a website, chat, issue, log or untrusted
application.

The metadata-only report is stored as `pending`. It does not complete the
assignment, change range allocation, stop the scheduler or mark the puzzle as
solved. Repeated reports for the same assignment are idempotent.

The report endpoint accepts exactly two fields:

```json
{
  "assignment_id": "<assignment UUID>",
  "client_id": "<anonymous client UUID>"
}
```

Private keys, solution contents, filesystem paths and raw engine output are
rejected by the server.

An operator must independently verify a potential solution using trusted
offline tooling and public blockchain data. A report may then be marked
`verified` or `rejected`. Marking a puzzle as solved remains a distinct
administrative action and is never triggered automatically by a client report.


## Filesystem protection

OpenPuzzle protects sensitive local storage using owner-only permissions:

- configuration and data directories: `0700`;
- identity, configuration, state, PID and SQLite files: `0600`;
- assignment workspaces: `0700`;
- engine result, log and lifecycle files: created under `umask 077`.

These permissions are applied both when files are created and when existing
local state is loaded.
