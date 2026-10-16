# GPU power and fan controls

The Linux Qt and Windows native panels have a separate **GPU power and fans**
card. It selects physical GPUs by vendor, PCI address and (for NVIDIA) UUID.
CUDA/OpenCL profile indexes are never used to select a board for a driver write.
The coordinator remains mandatory and has no editable server field.

Opening the panel, refreshing, changing language or selecting a GPU only reads
the driver. Each Apply confirmation identifies the board and requested value.
Only one property is changed per request. Values and support are rechecked in
the privileged helper immediately before the write, followed by driver readback.
No settings are automatically reapplied at startup. The driver determines
whether applied settings survive an application exit, driver reset or reboot.

## Backends and units

| Platform/vendor | Driver interface | Power | Fans |
| --- | --- | --- | --- |
| Linux/NVIDIA | NVML, system libnvidia-ml.so.1 | W; limited to the driver default maximum | All board fans; fixed % or driver automatic |
| Windows/NVIDIA | NVML, System32 nvml.dll | W; limited to the driver default maximum | All board fans, where the driver exposes these APIs |
| Linux/AMD | amdgpu hwmon, PCI device tree | W, converted from microWatts | pwm1, fixed % or pwm1_enable=2 |
| Windows/AMD | Optional locally installed ADLX SDK, System32 amdadlx64.dll | Percent offset, explicitly labelled % | Fixed fan curve or driver default curve/Zero RPM |

Support depends on the board and driver. Readable ranges do not prove that a
setter is permitted; permission/not-supported failures are reported on Apply.
Passive cards, ambiguous identities and missing APIs are not treated as working
controls. Missing AMD Linux power1_cap_default disables only Default power.
The regular Linux/Windows telemetry and thermal policy remain separate.

Manual fan targets are integer percentages, at least 30%, within driver limits,
and require a valid GPU temperature below 80 C when applied. Manual is a fixed
target, not a temperature curve managed by OpenPuzzle: monitor temperature and
use the existing thermal stop policy for searches. Auto restores driver fan
control. This feature does not implement an independent background fan watchdog
or guarantee emergency restoration after a process/OS crash.

NVIDIA snapshots every fan before applying. A failure or inconsistent readback
attempts to restore every fan's prior policy/target. Power changes restore the
prior limit if immediate readback fails. AMD sysfs snapshots PWM/mode or power;
ADLX snapshots the fan curve/Zero RPM or power. Rollback failures are reported
explicitly; they are never described as success. Do not operate another GPU
tuning utility concurrently; OpenPuzzle's lock serializes its own writers only.

## Permissions

The dedicated `openpuzzle-gpu-control` executable only supports `list` and
`apply` with a fixed whitelist of vendor, physical identity and numeric control
arguments. It does not initialize a client, load profiles, access the network,
claim assignments, change clocks/voltage, or perform a general tuning reset.

Windows Apply invokes `openpuzzle-gpu-control.exe` alongside the CLI through UAC.
The normal panel stays unprivileged; declining UAC changes nothing. The elevated
process is not forcibly terminated by the UI; closing waits for it to finish.
The panel independently reads the driver after it exits and checks the requested
setting before announcing success. A failed elevated helper can be diagnosed
with the same explicit `gpu-control apply` command in an administrator terminal.

Linux reads need no elevation. For Apply from an ordinary desktop session,
install the dedicated helper once, from a reviewed local build:

```sh
sudo bash OpenPuzzle/scripts/install-gpu-control-helper.sh \
  /absolute/path/to/build/gpu-control/openpuzzle-gpu-control
```

This installs only the root-owned helper and its polkit action; no GPU setting
is changed by installation. The panel uses `/usr/bin/pkexec` for each explicit
Apply and refuses a writable or non-root-owned helper. Do not run the whole
desktop panel as root. The CLI also accepts explicit administrator/root requests:

```sh
openpuzzle gpu-control list
sudo openpuzzle gpu-control apply --vendor nvidia \
  --pci 00000000:08:00.0 --uuid GPU-YOUR-ACTUAL-UUID --power 250
sudo openpuzzle gpu-control apply --vendor nvidia \
  --pci 00000000:08:00.0 --uuid GPU-YOUR-ACTUAL-UUID --fan auto
```

Use the actual identity and supported range from the read-only listing. Never
copy a fixture/example UUID into a hardware request.

## AMD Windows build option

ADLX headers are not redistributed in this source package. Install the official
AMD SDK separately under its own license, then configure with
`-DOPENPUZZLE_ADLX_INCLUDE_DIR=C:/path/to/ADLX/SDK/Include`, or pass
`-AdlxIncludeDir` to the Windows validation script. An empty option builds NVIDIA
controls and the shared UI without the AMD Windows backend. The source was
cross-compiled with official SDK commit
`32b5a740d42295c5dfe9026b9f52683da0f3af91`; AMD hardware writes remain unvalidated.
Runtime requires ADLX 1.4 interfaces for fan/power defaults. The adapter never
calls ResetToFactory: that would also alter unrelated GPU tuning. A driver
request for a broader reset is refused and reported.

## Validation boundary

The package validator runs injected NVML tests, isolated AMD sysfs fixtures,
shared parsing/identity/permission tests, desktop command fixtures and real
read-only capability inspection. It does not apply power or fan settings to real
hardware, repeat benchmarks, or request production assignments. Hardware control
proof requires a later explicit Apply and driver readback on each supported
vendor/platform. Compiling an adapter is not evidence of a successful hardware
write or improved speed/power efficiency.

Official references:

- https://docs.nvidia.com/deploy/nvml-api/latest/api/group__nvmlDeviceCommands.html
- https://docs.nvidia.com/deploy/nvml-api/latest/api/group__nvmlDeviceQueries.html
- https://kernel.org/doc/html/latest/gpu/amdgpu/thermal.html
- https://gpuopen.com/manuals/adlx/adlx-sdk-references/adlx-interfaces/gpu-tuning/iadlxmanualfantuning1/
- https://gpuopen.com/manuals/adlx/adlx-sdk-references/adlx-interfaces/gpu-tuning/iadlxmanualpowertuning1/

## Windows search speed

The current-status row also displays the last search speed in MKey/s, refreshed
with the selected execution slot. The source is the public progress already
read by ExecutionSyncService from the verified engine process generation. It
never substitutes a measured benchmark profile for an active search reading.
Idle, unknown, finished, solution-detected and missing/invalid progress states
show a dash. Switching slots clears the previous slot's metric.

The status CLI exposes only numeric `Speed MKey/s` and `Keys checked` fields.
No engine log content, solution files or private key material are displayed by
this metric. The existing progress/identity reader remains unchanged.

The portable launcher accepts `-DataReport` pointing at an existing validated
41-suite report with `ui-local-data`. This opens a new panel over the same
client data without copying state, starting an engine, acquiring a new runtime
owner or stopping the running search. Choose the CUDA measured profile to view
that slot. Keep the original runtime/engine files available while it runs.

## Linux packages

Normal Linux CMake installs and the DEB/TGZ packages include the dedicated
helper (`usr/libexec/openpuzzle-gpu-control`), the restricted polkit action
(`usr/share/polkit-1/actions/org.openpuzzle.gpu-control.policy`) and this guide.
CPack uses the `/usr` prefix to match the desktop entry, service and polkit
paths. Package staging and archive creation do not execute the helper, change
GPU settings, activate a service or start a search.

Installing the DEB as administrator installs the root-owned helper and policy.
Each Apply still asks for administrator authentication. A graphical polkit
agent and `/usr/bin/pkexec` must be available in the desktop session. There is
no password-free rule, setuid program, background tuning service or automatic
reapplication of settings.

Extracting the TGZ alone does not authorize writes. Portable users can continue
to install the dedicated helper explicitly with the included script, supplying
the extracted helper path. Existing GPU settings and client data are separate
from these installed program files and are not package contents.
