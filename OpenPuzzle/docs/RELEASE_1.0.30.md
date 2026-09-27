# OpenPuzzle 1.0.30 — Desktop thermal safety controls

OpenPuzzle 1.0.30 brings the thermal policy introduced in 1.0.28 into the Qt
desktop interface and adds a visible thermal state to every active GPU card.
Operators can configure and understand local protection without leaving the
interface, while the established command-line contract remains available.

## Desktop thermal configuration

The preferences window now exposes the same persisted thermal policy used by
`openpuzzle thermal`. Operators can:

- enable or disable thermal monitoring;
- set the warning temperature;
- set the higher critical temperature;
- select diagnostic-only monitoring; or
- request an orderly stop when an active GPU reaches the critical threshold.

The interface validates the supported ranges and requires the critical
threshold to remain above the warning threshold. Settings are disabled while
an execution is active. A saved change applies to the next GPU execution, so
the policy cannot change underneath a running worker.

Thermal protection remains disabled by default. Enabling monitoring alone is
diagnostic and cannot stop work unless orderly critical protection is also
selected explicitly.

## Per-GPU thermal state

`openpuzzle status` now derives a thermal state for every active GPU from its
current temperature and the persisted policy. Existing slot, speed, progress,
temperature and power fields remain unchanged.

The desktop interface shows the same state inside the matching runtime card:

- Normal uses a green badge;
- Warning uses an amber badge;
- Critical and Invalid use a red badge;
- Disabled and Unavailable use a neutral badge.

Labels are localized in English, Portuguese, French and Spanish. CUDA device
indexes retain their exact mapping. OpenCL state is displayed only when the
existing conservative physical-device association is unambiguous.

The state is observational. It does not alter clocks, fan curves or GPU power
limits. When explicitly enabled, critical protection continues to use the
existing orderly stop, synchronization and assignment-cancellation lifecycle.

## Locale-safe configuration

Persisted thermal thresholds use JSON decimal notation. Configuration parsing
is now independent of the desktop locale, so a value such as `73.5` is read as
73.5 degrees on systems that normally use a decimal comma. Existing integer
and decimal configurations remain compatible.

## Safety and compatibility

The 1.0.28 startup guard, active-runtime monitoring, recovery hysteresis and
diagnostic reporting are unchanged. The 1.0.29 optional heartbeat telemetry,
private Admin Nodes presentation and public API privacy boundary are also
unchanged.

BitCrack CUDA and OpenCL, KeyHunt CPU, optional PSCKangaroo routing,
multi-CUDA supervision, assignment fairness, client identity, the updater and
portable-package contracts remain compatible.

## Validation

The complete client suite contains 134 automated tests. Coverage includes:

- desktop policy loading, editing, validation and persistence;
- diagnostic-only and orderly-stop selection;
- active-runtime settings locking;
- locale-independent decimal configuration round trips;
- per-GPU Normal, Warning, Critical, Invalid, Disabled and Unavailable states;
- localized and colour-coded GPU-card presentation;
- runtime-status telemetry association;
- desktop control-process shutdown stability;
- existing runtime, telemetry, privacy and release contracts.

## Upgrade and validation

Allow active assignments to finish before replacing an installed client:

    openpuzzle safestop

Install the Debian package:

    sudo apt install ./OpenPuzzle-1.0.30-Linux-x86_64.deb

Confirm the installation, local sensors and current policy:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle thermal

Open `openpuzzle-ui`, configure the thermal policy in Preferences, then start
a GPU execution. Each active GPU card shows its temperature, power and current
thermal state. The same state can be inspected locally with:

    openpuzzle status

The portable updater package retains the established identity:

    OpenPuzzle-1.0.30-portable-XXXXXXXX.deb
