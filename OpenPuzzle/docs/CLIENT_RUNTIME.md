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

This telemetry layer is observational only. Client heartbeats include an
optional point-in-time sensor snapshot for the private administrative node
dashboard. Current readings are displayed beside the speed of the matching
active GPU slot. CUDA `cuda-N` slots use their exact physical device index;
generic and OpenCL slots are associated only when one unambiguous unused
physical reading remains. The dashboard never guesses between devices. The
public website and public network API do not expose these readings. The
payload keeps physical sensor identities separate from CUDA and
OpenCL capabilities, so one NVIDIA card exposed by both backends is not
duplicated. Missing tools and sensors are omitted rather than reported as
zero. Clients up to 1.0.28 remain compatible because the server treats the
telemetry field as optional.

Administrative reporting does not enable the thermal policy and never changes
clocks, fans or power limits. In supervised multi-GPU mode, exactly one worker
owns heartbeat telemetry collection; the remaining workers omit the optional
field and therefore cannot erase or duplicate the shared machine snapshot.
If that worker exits, the supervisor transfers ownership to one surviving
worker. A failed worker is not restarted and no GPU reset is attempted.
Thermal enforcement remains disabled unless explicitly configured below.

While a GPU execution is active, `openpuzzle status` appends its safely matched
temperature and current/configured power immediately after the speed. The Qt
desktop interface renders those fields in the corresponding runtime card.
Status also evaluates the persisted thermal policy for that GPU and reports
`DISABLED`, `UNAVAILABLE`, `NORMAL`, `WARNING`, `CRITICAL` or `INVALID`.
This evaluation is read-only and does not replace the runtime observer.
CUDA runtime device indexes map exactly to `cuda-N`; OpenCL is displayed only
when one physical sensor reading can be associated without ambiguity. CPU,
missing-sensor and ambiguous-device cards do not gain placeholder readings.
Status and interface telemetry remain read-only and do not alter the running
engine or its hardware configuration.

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
falls at least 2 degrees Celsius below the warning threshold. This recovery
hysteresis prevents warning/recovery oscillation near the boundary. A
temporarily unavailable sensor does not erase an existing hot state.
Configuration changes take effect when the next runtime is started.

Single-GPU execution owns its observer directly. Concurrent and multi-GPU
execution assigns observation to the parent supervisor; supervised workers are
explicitly excluded, so a six-GPU run still executes only one `nvidia-smi`
telemetry query per sample interval rather than one query per worker. NVIDIA
and AMD readings are collected in the same machine snapshot.

In the default diagnostic-only mode, runtime observation never signals, stops
or restarts an engine, cancels an assignment, or changes GPU power, clocks or
fans. Disabling the policy removes thermal-observer sampling; the independent
read-only heartbeat snapshot described above remains available to the private
administrative node dashboard.

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
is requested. Every execution mode reports `Startup blocked` and
`Assignment not requested` explicitly. Diagnostic-only mode still reports the
reading and continues. No startup telemetry is collected while the policy is
disabled.

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

The desktop interface exposes the persisted thermal policy without changing
the runtime contract. Operators can enable monitoring, edit the warning and
critical thresholds, and select diagnostic-only monitoring or an orderly stop
at the critical threshold. The same policy validator and private configuration
file are used by the CLI and UI. Thermal controls are read-only during an
active execution and saved changes apply to the next GPU runtime. Every active
GPU card carries a localized thermal-state badge: green for `NORMAL`, amber for
`WARNING`, red for `CRITICAL` or `INVALID`, and neutral for disabled or
unavailable monitoring.

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

Recovery distinguishes a confirmed stopped engine from an unreadable process
identity. A missing PID, a different boot or a reused PID/start time enters
interruption synchronization even while the recovering OpenPuzzle runtime owns
the slot. Without `exit.code`, this reports `cancelled` with exit code `-3` and
the last public progress; it never claims that the range was completed.

If a live process's identity cannot be read, synchronization preserves local
state and retries without uploading progress or completion. The presence or
absence of the main runtime does not prove whether that engine is alive.
Completion still requires the established engine proof and exact range count.

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

A failed runtime stop signal is not evidence that the runtime has exited.
The client keeps a complete runtime control marker when its process identity
cannot be read or a pidfd signal fails, allowing a later stop retry. For a
readable live identity, the marker continues to block a second runtime in the
same slot. Only a confirmed stale identity (absent PID, different boot or
start time) or an incomplete legacy marker is cleaned automatically.

When a runtime stop fails and its control marker is retained, `openpuzzle stop`
reports the affected slot and returns an error. It preserves assignment state
and does not fall back to stopping an engine behind the live supervisor. Other
slots still receive their own stop requests; a partial failure is reported.

Starting another continuous runtime also preserves a slot whose complete
runtime identity is temporarily unreadable. Failure to read the current boot
ID or the existing process start time is not evidence that the slot is free.
Acquisition fails before clearing its safe-stop request or replacing its PID
marker, and it leaves assignment state unchanged. The same check is repeated
if a marker appears during the exclusive-create attempt. Once the process is
confirmed absent, from another boot or a reused PID, normal stale cleanup and
acquisition can proceed; incomplete legacy markers keep their established
cleanup behavior. Read-only `running()` still means a positively verified
live identity, rather than presenting an uncertain identity as running.

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

### Unavailable process identity in runtime controls

`status` reports `identity unavailable` when the current boot ID, process start
identity or existence probe cannot be read reliably. This includes engine state
without a supervisor marker. A failed read does not mean `idle` or `stopped`.
`doctor` counts these slots under `Unknown identities` and emits `OP-DOCTOR-006`;
it reserves stale PID warnings for positively inactive controls. These read-only
checks include the primary, legacy and discovered `cuda-N` / `opencl-N` slots.

`safestop` checks all these runtime slots, including primary alongside concurrent
workers. An uncertain identity or failed request returns an error without claiming
that new assignments are blocked everywhere. Requests already made for other
slots remain in place, including requests from earlier commands. Repeated requests
are idempotent and never truncate an existing request. If identity becomes unreadable
after creating a request, that request is retained and the command reports failure;
a confirmed inactive runtime can still have its new request cleared.

Both regular installation and `update --safe` refuse uncertain runtime or engine
identities. Read-only `update --check` and `--download-only` remain available.
Installation checks include dynamic GPU slots and are repeated immediately before
installing. A safe update does not roll back existing requests when a peer request
fails, and it waits while a process remains active or its identity is unavailable.

### Unavailable process identity in the desktop interface

The desktop interface shows `Identity unavailable` when `status` reports an
uncertain engine or supervisor identity. This includes primary, legacy and
dynamic GPU slots, engine state without a supervisor marker, and a stopped
engine whose supervisor reports `Runtime identity... unavailable`.

Affected slot cards remain visible with an identity warning. The warning takes
precedence over the global running badge even when another slot is confirmed
active. A runtime PID alone does not override an explicit identity warning.
Starting searches, benchmarking, self-tests, Kangaroo installation and thermal
configuration remain disabled until a subsequent status confirms the state.
Refresh, diagnostics, update checks and auditing remain available. Stop controls
can retry the CLI's identity-checked requests; Kangaroo retains its safe-stop
restriction. Language changes preserve the warning and disabled controls.

### Desktop status query failures

The desktop interface waits for its first valid status response before enabling
new searches, benchmarks, self-tests, Kangaroo installation or thermal settings.
Normal command exit with code zero and recognized status for every reported slot
are required. The older supervisor-only output with a positive runtime PID remains
supported. Empty, malformed or incomplete output does not confirm an idle client.

A failed command, process error or ten-second status timeout shows `Status
unavailable` and blocks new work. Last confirmed assignment details, slot cards,
solution state and thermal readings/history remain intact. Diagnostic errors are
shown separately from these retained status details. Partial output from a timed
out or crashed status process is never accepted as a successful response.

Refresh and diagnostics remain available when the CLI can be started. Stop
controls retain their established availability for the last confirmed occupied
slots. The interface does not report a failed query as a stopped execution, and
language changes preserve the failure reason. A later valid response restores
normal controls or the explicit process-identity warning described above.

### Desktop command reservation

The desktop interface reserves its foreground command until that command exits
or fails to start. Background status replies, query failures and query timeouts
do not release this reservation. New searches, other commands, execution choices
and thermal configuration remain disabled while the foreground command is in
progress; status polling continues to update the visible runtime state.

The pending detached runtime launch has a separate flag and retains its existing
status-based release. A completed foreground command releases only its own
reservation, including normal exit, nonzero exit, process crash and failure to
start. A status timeout terminates only the status subprocess and leaves the
foreground command running. Language changes preserve disabled controls. Later
control availability follows the last confirmed runtime and identity state.

### Desktop launch and status query ordering

Each desktop status query records the successful detached launch generation at
which it starts. A query started before a new launch cannot settle that launch's
pending reservation. Its output, process errors and timeout do not replace the
last confirmed status, runtime cards, solution notice or thermal readings, and
they do not enable execution or configuration controls. Language changes keep
the reservation. A timeout still terminates only the status subprocess.

After an older query finishes, the interface schedules another query. A query
started after the launch retains the established behavior: a valid response
updates the runtime state and control availability, while a failed response
reports unavailable status and leaves new searches disabled until a valid query
recovers. This ordering does not prove runtime registration or process liveness;
it prevents a pre-launch query from being treated as a post-launch observation.
The foreground command reservation remains independent.

### Confirmation dialogs and persisted client data

Stop and Kangaroo installation confirmations recheck the current availability
of their controls after the dialog closes. Status polling continues while a
confirmation is open. A newly occupied or unavailable client cannot proceed
with installation, and a newly idle client does not receive the confirmed Stop
command. Existing Stop retries for occupied slots with unavailable identity or
status remain supported. Cancelling a confirmation performs no command.

Thermal history loading treats malformed entries as a recovery condition for
the entire file, including malformed entries after valid ones. Existing in-memory
history and the original file remain intact; appending stays blocked until an
explicit clear. Missing optional temperature and stop-request fields retain their
legacy defaults, while fields with invalid types are rejected.

Configuration string values are decoded as JSON fields, preserving quoted paths,
backslashes, control characters and escaped Unicode. Existing nested and flat
legacy configuration layouts remain supported. Configuration and execution
metadata writers escape string contents consistently. Recovery decodes execution
strings without changing their original command contents. These changes do not
alter engine selection, assignment allocation or execution of recovered commands.


### Complete local files and strict numeric recovery

Configuration loading parses the complete JSON document before applying any field.
An incomplete document returns the established defaults without rewriting the file;
numeric fields with trailing text, fractional device IDs or non-finite values do not
become partial settings. Flat and nested legacy layouts remain readable. Execution
recovery also reads complete JSON fields: whitespace and field order do not change
status or `echo_output`, and a later command containing `true` cannot override a
stored `false`. Invalid recovery counters retain their unavailable defaults.

Configuration, execution metadata and client-state writers serialize numbers with
the classic locale. JSON decimals use a dot and integers have no regional grouping;
finite doubles retain round-trip precision. Non-finite configuration values are
rejected, and non-finite execution speeds do not replace a previous state file.

These writers publish a complete, owner-only temporary file by renaming it within
the destination directory. Failed writes, flushes or renames preserve the prior
file; cleanup removes only the temporary file created by that operation. Temporary
names are unique for each writer. Each file is published separately: this is not a
transaction across execution.json and state.json and does not promise directory
persistence across a sudden power loss. ExecutionPersistence retains its existing
void API and silent failure reporting; configuration and client-state saves return
false on failure.

Client-state escaping preserves carriage returns, newlines and literal backslashes.
Strict unsigned identity parsing treats negative, signed, overflowing or partially
numeric start times as unavailable (zero), while keeping the assignment visible.
Missing legacy identity fields retain the same unavailable defaults. Tests exercise
these cases only in temporary directories; short writes are induced in isolated
child processes with a file-size limit and never launch an engine or assignment.

### Stable local client identity

The first client registration creates `~/.config/OpenPuzzle/client.id` under a
private, persistent `client.id.lock` file. Cooperating client processes serialize
both reading and first creation with an exclusive process lock. All simultaneous
first callers receive the same complete UUID; the lock file is never removed
after use, so later callers cannot accidentally lock a different inode.

Only an absent identity file allows UUID generation. Empty, malformed, unreadable
or non-regular identity files return a controlled failure and retain their bytes.
A valid UUID with no line ending, LF or CRLF is accepted without changing its
text or case. Extra records or invalid hexadecimal characters are rejected.
Identity and lock symlinks are rejected without changing their targets.

UUID generation uses the classic locale. Atomic private publication prevents a
failed write from leaving a partial identity that a later call might register.
The existing atomic writer does not provide a transaction across multiple files
or a guarantee that directory entries survive a power loss. A failed creation
can be retried when the storage problem is resolved.

An existing damaged identity is not silently reset: preserve it and recover the
original UUID from a known backup or matching local assignment before retrying.
This change does not rotate existing valid UUIDs or register any client during
validation. Mixed client versions that do not use the lock are not covered by
the serialization guarantee.
