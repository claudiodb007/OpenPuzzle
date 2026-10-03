# OpenPuzzle 1.0.32 — Local thermal history

OpenPuzzle 1.0.32 adds a persistent local history of GPU thermal-state
transitions to the Qt desktop interface. Operators can review when a runtime
slot entered Warning, Critical or Invalid state, when it recovered and which
critical-protection action was active.

## Persistent transition history

The desktop details panel now includes a translated Thermal history tab. Each
record contains:

- the local date and time of the transition;
- the affected runtime slot, such as `cuda-0`, `cuda-4` or `opencl`;
- the new thermal state;
- the reported temperature, when available; and
- the critical-protection action that applied at that moment.

Warning, Critical, Invalid and recovery transitions are recorded. Unchanged
device states do not generate duplicate entries during three-second polling or
after the desktop interface is restarted.

## Bounded local storage

The newest 200 events are retained in a human-readable JSON file:

    ~/.local/share/OpenPuzzle/thermal-history.json

The file and its containing directory are restricted to the current user. A
translated Clear thermal history action removes the complete local history
only after explicit confirmation. Clearing history does not change the active
thermal policy or affect a running assignment.

## Privacy boundary

Thermal history remains entirely on the client computer. It is not included
in heartbeats, uploaded to the coordination server, displayed in the private
Admin Nodes dashboard or exposed through the public website and network-status
API.

The existing optional heartbeat telemetry remains a current snapshot only and
keeps its established private-server boundary.

## Localization

The history tab, empty-state message, state names, protection actions, clear
button and confirmation dialog are available in English, Portuguese, French
and Spanish. Runtime slot identities remain unchanged so every event maps
directly to the corresponding GPU card and command-line status.

## Safety and compatibility

Thermal history is observational and does not:

- change GPU clocks, fan curves or power limits;
- enable a disabled thermal policy;
- request assignments or start or stop an execution;
- replace the persistent thermal alert introduced in 1.0.31; or
- create a new runtime shutdown path.

The established runtime thermal observer remains responsible for diagnostic
monitoring, hysteresis, startup protection and the optional orderly stop at
the critical threshold. The policy remains disabled by default and critical
shutdown remains opt-in.

BitCrack CUDA and OpenCL, KeyHunt CPU, optional PSCKangaroo routing,
multi-CUDA supervision, assignment fairness, desktop thermal controls,
private heartbeat telemetry, the updater and portable-package contracts remain
compatible.

## Validation

The complete client suite contains 136 automated tests. Coverage includes:

- Warning, Critical, Invalid and recovery event persistence;
- local timestamp, runtime slot, temperature and protection-action fields;
- deduplication of unchanged state after interface restart;
- newest-200-event bounded retention;
- owner-only file and directory permissions;
- confirmed complete-history removal;
- English, Portuguese, French and Spanish presentation; and
- existing runtime, telemetry, privacy, desktop and release contracts.

## Upgrade and validation

Allow active assignments to finish before replacing an installed client:

    openpuzzle safestop

Install the Debian package:

    sudo apt install ./OpenPuzzle-1.0.32-Linux-x86_64.deb

Confirm the installation, local sensors and active policy:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle thermal

Open `openpuzzle-ui` and select Thermal history under the details panel. A new
installation shows the localized empty state until the first monitored thermal
transition occurs. Existing runtime status, persistent alerts and per-GPU
thermal badges continue to operate independently of the stored history.

The portable updater package retains the established identity:

    OpenPuzzle-1.0.32-portable-XXXXXXXX.deb
