# OpenPuzzle 1.0.31 — Desktop thermal alerts

OpenPuzzle 1.0.31 adds persistent, localized thermal alerts to the Qt desktop
interface. Operators can immediately see which active GPU needs attention,
its reported temperature and whether critical protection requested an orderly
stop.

## Persistent per-GPU alerts

The runtime dashboard now shows an alert above the active slot cards whenever
one or more monitored GPUs report:

- Warning, using an amber alert;
- Critical, using a red alert; or
- Invalid, using a separate red sensor-warning alert.

Every affected runtime slot and its reported temperature are listed. When
several GPUs require attention simultaneously, the banner adopts the most
urgent state while retaining the complete device list.

Critical alerts distinguish between diagnostic-only monitoring and the
configured orderly stop. The interface reports the action selected by the
active policy; it does not create a separate shutdown path.

## Transition and recovery reporting

The interface refreshes `openpuzzle status` every three seconds. A warning,
critical condition, invalid sensor or return to Normal is announced once when
that transition occurs. Unchanged readings do not repeatedly create notices.

The persistent alert remains visible while an affected GPU remains in an
alert state. It disappears after every monitored GPU returns to Normal, and
the recovery is announced once in the interface status bar.

## Localization

Alert titles, descriptions, protection actions and transition notices are
available in English, Portuguese, French and Spanish. Runtime slot identities
such as `cuda-0`, `cuda-4` and `opencl` remain unchanged so the alert maps
directly to the corresponding GPU card and command-line status.

## Safety and compatibility

Desktop alerts are local and read-only. They do not:

- change GPU clocks, fan curves or power limits;
- enable a disabled thermal policy;
- request assignments or start an execution;
- send additional data to the coordination server; or
- expose telemetry through the public website or network-status API.

The established runtime thermal observer remains responsible for diagnostic
monitoring, hysteresis, startup protection and the optional orderly stop at
the critical threshold. The policy remains disabled by default and critical
shutdown remains opt-in.

BitCrack CUDA and OpenCL, KeyHunt CPU, optional PSCKangaroo routing,
multi-CUDA supervision, assignment fairness, private heartbeat telemetry, the
updater and portable-package contracts remain compatible.

## Validation

The complete client suite contains 135 automated tests. Coverage includes:

- Warning, Critical and Invalid alert parsing;
- multiple affected GPU slots and priority selection;
- transition deduplication during three-second polling;
- return-to-Normal recovery reporting;
- diagnostic-only and orderly-stop wording;
- localized desktop presentation;
- asynchronous status refresh synchronization;
- existing runtime, telemetry, privacy and release contracts.

## Upgrade and validation

Allow active assignments to finish before replacing an installed client:

    openpuzzle safestop

Install the Debian package:

    sudo apt install ./OpenPuzzle-1.0.31-Linux-x86_64.deb

Confirm the installation, local sensors and active policy:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle thermal

Open `openpuzzle-ui` and start a GPU execution. Normal operation retains the
per-GPU thermal badge introduced in 1.0.30. If a monitored GPU enters Warning,
Critical or Invalid state, the new persistent alert appears above the runtime
dashboard and identifies the affected slot.

The portable updater package retains the established identity:

    OpenPuzzle-1.0.31-portable-XXXXXXXX.deb
