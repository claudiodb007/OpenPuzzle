# Native Windows panel

Source base: `3e28a2a8913b345f6639f42b0755cfeb2cb528b5`.

The native Win32 panel is a first Windows interface over the validated console
client. It builds in the isolated MSVC platform target with the existing Windows
SDK; it does not require Qt or replace the Linux Qt interface.

`openpuzzle-ui.exe` requires the sibling `openpuzzle.exe`, `engines/` and measured
`profiles/{cpu,cuda,opencl}/result.json`, `puzzles/71.json` and the original
Linux header logo in `ui-assets/`. It discovers these profiles locally and
also accepts a profile selected through a file dialog. GPU profiles require a
physical UUID or PCI binding. The displayed engine/device parameters come from
the measured profile; the panel cannot override them or launch Kangaroo.

The panel supports an explicit server URL, positive puzzle number, duration,
one-assignment mode, local preflight, start, cooperative stop, finish-current
assignment, recovery, read-only NVML telemetry and subsequent-run thermal policy.
CPU uses the primary slot; CUDA and OpenCL use their corresponding device slots.
The slot can be selected independently for status/control/recovery of preserved
work, while launch requires a slot matching the selected profile.

Each command runs the sibling console client with direct `CreateProcessW`, an
absolute application name and Microsoft argv quoting. There is no shell and no
mutation of the parent execution-slot environment. Only the pipe and NUL input
handles are inherited. Timer-based polling drains bounded output per tick,
retains a bounded log tail and keeps runtime, control, status and sensor channels
independent. Runtime has no timeout. Short commands have a timeout and terminate
only their owned console process. The UI does not kill a search runtime.

The panel mirrors the Linux card order, original header logo, light/dark palettes,
primary/secondary/stop button styling, scrollable page and monospace details.
Thermal thresholds accept decimal dot or comma. Themes are selectable per session.
Profile preflight ignores server/puzzle/duration fields and always uses a local
dry-run; Start retains its explicit puzzle and server, and requires local metadata.

Opening the panel only reads local profiles, status, sensors and thermal policy.
Unknown state blocks start. A stop acknowledgement is shown in the log and never
interpreted as proof of runtime termination. Close is blocked during an owned
runtime or control operation; the operator must stop and wait for completion.
An externally running slot is displayed, but closing a viewing window does not
stop it. Recovery is explicit and reconciles prior work without claiming again.
Start and recovery require an explicit UI confirmation.

The portable launcher uses report-local `ui-local-data` for settings and identity,
preserving the installed profile. The server field is initially blank. Thermal
settings are saved only by the explicit button, through the tested CLI command,
and apply to subsequent runs. No drivers, power limits or fans are changed.

The UI is not yet the complete Windows application: Persistent preferences,
charts/history, autostart integration, Kangaroo, an installer and production
validation remain pending. Multiple identical GPUs are covered by fixtures;
real multi-GPU Windows hardware remains a separate validation requirement.

## Validation

The package validator requires the successful physical binding report and runs
37 registered Windows suites with JUnit equality/no-skips checks. New coverage:
portable profile/slot/state/thermal command model; native argv streaming and
timeout/control isolation; hidden native window creation, read-only fixture
commands and close blocking during a synthetic runtime. It preserves engine and
profile bytes and repeats real engine one-key synthetic assignment regression
with isolated thermal protection through UI-generated commands, explicit puzzle
metadata and matching per-backend slots. Missing metadata is rejected before
identity/claim. Original Linux Puzzle 71 metadata is included byte-identically. No benchmark or production assignment occurs.

Windows visual review is separate: open `portable/Start-OpenPuzzleUi.ps1` from the
new report, check profile selection, UUID/PCI, sensor readings, resizing and local
preflight with the server field empty. Do not click Start for that local review.
Native MSVC execution and visual review must be reported separately from Linux
portable execution. The Linux integration script checks 170 complete Linux tests
and 18 portable tests before creating the isolated UI commit.

Process API reference: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
